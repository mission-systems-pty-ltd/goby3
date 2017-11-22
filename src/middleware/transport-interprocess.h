#ifndef TransportInterProcess20160622H
#define TransportInterProcess20160622H

#include <sys/types.h>
#include <unistd.h>
#include <functional>
#include <thread>
#include <atomic>
#include <tuple>

#include "transport-common.h"
#include "goby/middleware/protobuf/interprocess_config.pb.h"

namespace goby
{   
    template<typename Derived, typename InnerTransporter>
        class InterProcessTransporterBase :
        public StaticTransporterInterface<InterProcessTransporterBase<Derived, InnerTransporter>, InnerTransporter>,
        public Poller<InterProcessTransporterBase<Derived, InnerTransporter>>
    {
        using PollerType = Poller<InterProcessTransporterBase<Derived, InnerTransporter>>;
        
    public:
    InterProcessTransporterBase(InnerTransporter& inner) :
        PollerType(&inner), inner_(inner)
            { }
        
    InterProcessTransporterBase(InnerTransporter* inner_ptr = new InnerTransporter,
                                bool base_owns_inner = true) :
        PollerType(inner_ptr),
            own_inner_(base_owns_inner ? inner_ptr : nullptr),
            inner_(*inner_ptr)
            { }

	template<typename Data>
	    static constexpr int scheme()
	{ return goby::scheme<Data>(); }

	
        // RUNTIME groups
        template<typename Data, int scheme = scheme<Data>()>
            void publish_dynamic(const Data& data, const Group& group, const goby::protobuf::TransporterConfig& transport_cfg = goby::protobuf::TransporterConfig())
            {
                check_validity_runtime(group);
                static_cast<Derived*>(this)->template _publish<Data, scheme>(data, group, transport_cfg);
                inner_.template publish_dynamic<Data, scheme>(data, group, transport_cfg);
            }

        template<typename Data, int scheme = scheme<Data>()>
            void publish_dynamic(std::shared_ptr<const Data> data, const Group& group, const goby::protobuf::TransporterConfig& transport_cfg = goby::protobuf::TransporterConfig())
            {
                if(data)
                {
                    check_validity_runtime(group);
                    static_cast<Derived*>(this)->template _publish<Data, scheme>(*data, group, transport_cfg);
                    inner_.template publish_dynamic<Data, scheme>(data, group, transport_cfg);
                }
            }

        template<typename Data, int scheme = scheme<Data>()>
            void publish_dynamic(std::shared_ptr<Data> data, const Group& group, const goby::protobuf::TransporterConfig& transport_cfg = goby::protobuf::TransporterConfig())
            { publish_dynamic<Data, scheme>(std::shared_ptr<const Data>(data), group, transport_cfg); }
        
        template<typename Data, int scheme = scheme<Data>()>
            void subscribe_dynamic(std::function<void(const Data&)> f, const Group& group)
        {
            check_validity_runtime(group);
            static_cast<Derived*>(this)->template _subscribe<Data, scheme>([=](std::shared_ptr<const Data> d) { f(*d); }, group);
        }
        
        template<typename Data, int scheme = scheme<Data>()>
            void subscribe_dynamic(std::function<void(std::shared_ptr<const Data>)> f, const Group& group)
        {
            check_validity_runtime(group);
            static_cast<Derived*>(this)->template _subscribe<Data, scheme>(f, group);
        }

        // Wildcards
        void subscribe_regex(std::function<void(const std::vector<unsigned char>&, int scheme, const std::string& type, const Group& group)> f,
                             const std::set<int>& schemes,
                             const std::string& type_regex = ".*",
                             const std::string& group_regex = ".*")
        {
            static_cast<Derived*>(this)->template _subscribe_regex(f, schemes, type_regex, group_regex);
        }
        
        
        std::unique_ptr<InnerTransporter> own_inner_;
        InnerTransporter& inner_;
        static constexpr Group forward_group_ { "goby::InterProcessForwarder" };
        static constexpr Group regex_group_ { "goby::InterProcessRegexData" };

    private:  
        friend PollerType;
        int _poll()
        { return static_cast<Derived*>(this)->_poll(); }
    };    
    
    template<typename Derived, typename InnerTransporter>
        constexpr goby::Group InterProcessTransporterBase<Derived, InnerTransporter>::forward_group_;
    template<typename Derived, typename InnerTransporter>
        constexpr goby::Group InterProcessTransporterBase<Derived, InnerTransporter>::regex_group_;
    
    template<typename InnerTransporter>
        class InterProcessForwarder : public InterProcessTransporterBase<InterProcessForwarder<InnerTransporter>, InnerTransporter>
    {
    public:
        using Base = InterProcessTransporterBase<InterProcessForwarder<InnerTransporter>, InnerTransporter>;

        InterProcessForwarder(InnerTransporter& inner) : Base(inner)
        {
            Base::inner_.template subscribe<Base::regex_group_, goby::protobuf::SerializerTransporterData>(
                [this](std::shared_ptr<const goby::protobuf::SerializerTransporterData> d) { _receive_regex_data_forwarded(d);});
        }

        friend Base;
    private:
        template<typename Data, int scheme>
            void _publish(const Data& d, const Group& group, const goby::protobuf::TransporterConfig& transport_cfg)
        {
            // create and forward publication to edge
            std::vector<char> bytes(SerializerParserHelper<Data, scheme>::serialize(d));
            std::string* sbytes = new std::string(bytes.begin(), bytes.end());
            std::shared_ptr<goby::protobuf::SerializerTransporterData> data = std::make_shared<goby::protobuf::SerializerTransporterData>();

            data->set_marshalling_scheme(scheme);
            data->set_type(SerializerParserHelper<Data, scheme>::type_name());
            data->set_group(std::string(group));
            data->set_allocated_data(sbytes);
        
            *data->mutable_cfg() = transport_cfg;

            Base::inner_.template publish<Base::forward_group_>(data);
        }
        
        template<typename Data, int scheme>
            void _subscribe(std::function<void(std::shared_ptr<const Data> d)> f, const Group& group)
        {
            Base::inner_.template subscribe_dynamic<Data, scheme>(f, group);

            // forward subscription to edge
            auto inner_publication_lambda = [&](std::shared_ptr<const Data> d, const goby::protobuf::TransporterConfig& t) { Base::inner_.template publish_dynamic<Data, scheme>(d, group, t); };
            typename SerializationSubscription<Data, scheme>::HandlerType inner_publication_function(inner_publication_lambda);

            auto subscription = std::shared_ptr<SerializationSubscriptionBase>(
                new SerializationSubscription<Data, scheme>(inner_publication_function,
                                                            group,
                                                            [=](const Data&d) { return group; }));
                    
            Base::inner_.template publish<Base::forward_group_, SerializationSubscriptionBase>(subscription);

        }
        
        void _subscribe_regex(std::function<void(const std::vector<unsigned char>&, int scheme, const std::string& type, const Group& group)> f,
                              const std::set<int>& schemes,
                              const std::string& type_regex = ".*",
                              const std::string& group_regex = ".*")
        {
            auto inner_publication_lambda = [&](const std::vector<unsigned char>& data, int scheme, const std::string& type, const Group& group)
                {
                    std::shared_ptr<goby::protobuf::SerializerTransporterData> forwarded_data(new goby::protobuf::SerializerTransporterData);
                    forwarded_data->set_marshalling_scheme(scheme);
                    forwarded_data->set_type(type);
                    forwarded_data->set_group(group);
                    forwarded_data->set_data(std::string(data.begin(), data.end()));
                    Base::inner_.template publish<Base::regex_group_, goby::protobuf::SerializerTransporterData>(forwarded_data);
                };
            typename SerializationSubscriptionRegex::HandlerType inner_publication_function(inner_publication_lambda);

            auto portal_subscription = std::shared_ptr<SerializationSubscriptionRegex>(
                new SerializationSubscriptionRegex(inner_publication_function,
                                                   schemes,
                                                   type_regex,
                                                   group_regex));
            Base::inner_.template publish<Base::forward_group_, SerializationSubscriptionRegex>(portal_subscription);


            auto local_subscription = std::shared_ptr<SerializationSubscriptionRegex>(
                new SerializationSubscriptionRegex(f,
                                                   schemes,
                                                   type_regex,
                                                   group_regex));
            regex_subscriptions_.insert(local_subscription);
        }
        
        void _receive_regex_data_forwarded(std::shared_ptr<const goby::protobuf::SerializerTransporterData> data)
        {
            const auto& bytes = data->data();
            for(auto& sub: regex_subscriptions_)
                sub->post(bytes.begin(), bytes.end(), data->marshalling_scheme(), data->type(), data->group());
        }
        
        int _poll()
        { return 0; } // A forwarder is a shell, only the inner Transporter has data

    private:
        std::set<std::shared_ptr<const SerializationSubscriptionRegex>> regex_subscriptions_;
    };    

}

#endif