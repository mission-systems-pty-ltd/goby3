#include <cmath>
#include <string>
#include <vector>

#include <boost/asio.hpp>

#include "goby/middleware/marshalling/protobuf.h"

#include "goby/middleware/io/serial_mavlink.h"
#include "goby/zeromq/application/multi_thread.h"
#include "goby/zeromq/protobuf/mavlink_serial_gateway_config.pb.h"

using AppBase =
    goby::zeromq::MultiThreadApplication<goby::apps::zeromq::protobuf::MAVLinkSerialGatewayConfig>;
using ThreadBase =
    goby::middleware::SimpleThread<goby::apps::zeromq::protobuf::MAVLinkSerialGatewayConfig>;
namespace si = boost::units::si;

using goby::glog;

namespace goby
{
namespace apps
{
namespace zeromq
{
namespace groups
{
constexpr goby::middleware::Group mavlink_raw_in{"goby::apps::zeromq::mavlink_raw_in"};
constexpr goby::middleware::Group mavlink_raw_out{"goby::apps::zeromq::mavlink_raw_out"};
} // namespace groups

class MAVLinkSerialGateway : public AppBase
{
  public:
    using SerialThread =
        goby::middleware::io::SerialThreadMAVLink<groups::mavlink_raw_in, groups::mavlink_raw_out>;

    MAVLinkSerialGateway() { launch_thread<SerialThread>(cfg().serial()); }
};

class MAVLinkSerialGatewayConfigurator
    : public goby::middleware::ProtobufConfigurator<
          goby::apps::zeromq::protobuf::MAVLinkSerialGatewayConfig>
{
  public:
    MAVLinkSerialGatewayConfigurator(int argc, char* argv[])
        : goby::middleware::ProtobufConfigurator<
              goby::apps::zeromq::protobuf::MAVLinkSerialGatewayConfig>(argc, argv)
    {
        goby::apps::zeromq::protobuf::MAVLinkSerialGatewayConfig& cfg = mutable_cfg();
        goby::middleware::protobuf::SerialConfig& serial_cfg = *cfg.mutable_serial();

        // set default baud
        if (!serial_cfg.has_baud())
            serial_cfg.set_baud(57600);
    }
};
} // namespace zeromq
} // namespace apps
} // namespace goby

int main(int argc, char* argv[])
{
    return goby::run<goby::apps::zeromq::MAVLinkSerialGateway>(
        goby::apps::zeromq::MAVLinkSerialGatewayConfigurator(argc, argv));
}
