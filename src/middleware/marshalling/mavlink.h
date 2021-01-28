// Copyright 2019-2020:
//   GobySoft, LLC (2013-)
//   Community contributors (see AUTHORS file)
// File authors:
//   Toby Schneider <toby@gobysoft.org>
//
//
// This file is part of the Goby Underwater Autonomy Project Libraries
// ("The Goby Libraries").
//
// The Goby Libraries are free software: you can redistribute them and/or modify
// them under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 2.1 of the License, or
// (at your option) any later version.
//
// The Goby Libraries are distributed in the hope that they will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Goby.  If not, see <http://www.gnu.org/licenses/>.

#ifndef GOBY_MIDDLEWARE_MARSHALLING_MAVLINK_H
#define GOBY_MIDDLEWARE_MARSHALLING_MAVLINK_H

#include <array>         // for array<>::iterator
#include <cstdint>       // for uint8_t, uint32_t
#include <memory>        // for shared_ptr, make_sh...
#include <mutex>         // for mutex, lock_guard
#include <ostream>       // for basic_ostream, endl
#include <string>        // for operator<<, string
#include <tuple>         // for tuple, make_tuple
#include <type_traits>   // for enable_if, is_base_of
#include <unordered_map> // for unordered_map, oper...
#include <utility>       // for make_pair, pair
#include <vector>        // for vector

#include <mavlink/v2.0/common/common.hpp>

#include "goby/util/debug_logger/flex_ostream.h" // for operator<<, FlexOst...

#include "interface.h" // for MarshallingScheme

namespace goby
{
namespace middleware
{
/// \brief A registry of mavlink types used for decoding
///
/// You should register the MESSAGE_ENTRIES for the dialect(s) you're using with this registry, if other than common and minimal. Parsing and serialization will still work if you don't, but you will get CRC errors for the unknown types.
struct MAVLinkRegistry
{
    /// \brief Register a new Mavlink dialect
    ///
    /// \param entries Mavlink autogenerated MESSAGE_ENTRIES, i.e. mavlink::my_dialect::MESSAGE_ENTRIES
    template <std::size_t Size>
    static void register_dialect_entries(std::array<mavlink::mavlink_msg_entry_t, Size> entries)
    {
        std::lock_guard<std::mutex> lock(mavlink_registry_mutex_);
        for (const auto& entry : entries) entries_.insert(std::make_pair(entry.msgid, entry));
    }

    /// \brief Retrieve a entry given a message id
    ///
    /// \param msgid Mavlink message id to look up
    /// \return The mavlink_msg_entry_t for the given msgid or nullptr if the msgid isn't loaded.
    static const mavlink::mavlink_msg_entry_t* get_msg_entry(uint32_t msgid)
    {
        if (entries_.empty())
            register_default_dialects();

        std::lock_guard<std::mutex> lock(mavlink_registry_mutex_);
        auto it = entries_.find(msgid);
        if (it != entries_.end())
            return &it->second;
        else
            return nullptr;
    }

    static void register_default_dialects();

  private:
    static std::unordered_map<uint32_t, mavlink::mavlink_msg_entry_t> entries_;
    static std::mutex mavlink_registry_mutex_;
};

/// \brief Specialization for Mavlink message using runtime introspection (publish and subscribe_type_regex only)
template <> struct SerializerParserHelper<mavlink::mavlink_message_t, MarshallingScheme::MAVLINK>
{
    static std::vector<char> serialize(const mavlink::mavlink_message_t& msg)
    {
        std::array<uint8_t, MAVLINK_MAX_PACKET_LEN> buffer;
        auto length = mavlink::mavlink_msg_to_send_buffer(&buffer[0], &msg);
        return std::vector<char>(buffer.begin(), buffer.begin() + length);
    }

    static std::string type_name(const mavlink::mavlink_message_t& msg)
    {
        return std::to_string(msg.msgid);
    }

    template <typename CharIterator>
    static std::shared_ptr<mavlink::mavlink_message_t>
    parse(CharIterator bytes_begin, CharIterator bytes_end, CharIterator& actual_end,
          const std::string& type)
    {
        CharIterator c = bytes_begin;
        auto msg = std::make_shared<mavlink::mavlink_message_t>();
        mavlink::mavlink_message_t msg_buffer{};
        mavlink::mavlink_status_t status{}, status_buffer{};
        while (c != bytes_end)
        {
            auto res = mavlink::mavlink_frame_char_buffer(&msg_buffer, &status_buffer, *c,
                                                          msg.get(), &status);

            switch (res)
            {
                case mavlink::MAVLINK_FRAMING_INCOMPLETE: ++c; break;

                case mavlink::MAVLINK_FRAMING_OK:
                {
                    actual_end = c;
                    return msg;
                }

                case mavlink::MAVLINK_FRAMING_BAD_CRC:
                    if (!MAVLinkRegistry::get_msg_entry(msg->msgid))
                    {
                        goby::glog.is_debug2() &&
                            goby::glog << "MAVLink msg type: " << type
                                       << " is unknown, so unable to validate CRC" << std::endl;
                    }
                    else
                    {
                        goby::glog.is_warn() &&
                            goby::glog << "BAD CRC decoding MAVLink type: " << type << std::endl;
                    }
                    goto fail;
                    break;
                case mavlink::MAVLINK_FRAMING_BAD_SIGNATURE:
                    goby::glog.is_warn() &&
                        goby::glog << "BAD SIGNATURE decoding MAVLink type: " << type << std::endl;
                    goto fail;

                    break;
                default:
                    goby::glog.is_warn() && goby::glog
                                                << "Unknown value " << res
                                                << " returned while decoding MAVLink type: " << type
                                                << std::endl;
                    goto fail;

                    break;
            }
        }
    fail:
        actual_end = bytes_end;
        return msg;
    }
};

struct MAVLinkTupleIndices
{
    enum
    {
        SYSTEM_ID_INDEX = 0,
        COMPONENT_ID_INDEX = 1,
        PACKET_INDEX = 2
    };
};

/// \brief Specialization for known compile-time Mavlink message and system id / component id metadata, e.g. DataType == HEARTBEAT, with tuple of <sysid, compid, msg>
template <typename DataType, typename Integer>
struct SerializerParserHelper<std::tuple<Integer, Integer, DataType>, MarshallingScheme::MAVLINK>
{
    static std::vector<char>
    serialize(const std::tuple<Integer, Integer, DataType>& packet_with_metadata)
    {
        mavlink::mavlink_message_t msg{};
        mavlink::MsgMap map(msg);
        const DataType& packet = std::get<MAVLinkTupleIndices::PACKET_INDEX>(packet_with_metadata);
        packet.serialize(map);
        mavlink::mavlink_finalize_message(
            &msg, std::get<MAVLinkTupleIndices::SYSTEM_ID_INDEX>(packet_with_metadata),
            std::get<MAVLinkTupleIndices::COMPONENT_ID_INDEX>(packet_with_metadata),
            packet.MIN_LENGTH, packet.LENGTH, packet.CRC_EXTRA);
        return SerializerParserHelper<mavlink::mavlink_message_t,
                                      MarshallingScheme::MAVLINK>::serialize(msg);
    }

    // use numeric type name since that's all we have with mavlink_message_t alone
    static std::string type_name() { return std::to_string(DataType::MSG_ID); }
    static std::string type_name(const std::tuple<Integer, Integer, DataType>& /*d*/)
    {
        return type_name();
    }

    template <typename CharIterator>
    static std::shared_ptr<std::tuple<Integer, Integer, DataType>>
    parse(CharIterator bytes_begin, CharIterator bytes_end, CharIterator& actual_end,
          const std::string& /*type*/ = type_name())
    {
        auto msg =
            SerializerParserHelper<mavlink::mavlink_message_t, MarshallingScheme::MAVLINK>::parse(
                bytes_begin, bytes_end, actual_end, type_name());
        auto packet_with_metadata = std::make_shared<std::tuple<Integer, Integer, DataType>>();
        DataType* packet = &std::get<MAVLinkTupleIndices::PACKET_INDEX>(*packet_with_metadata);
        mavlink::MsgMap map(msg.get());
        packet->deserialize(map);
        std::get<MAVLinkTupleIndices::SYSTEM_ID_INDEX>(*packet_with_metadata) = msg->sysid;
        std::get<MAVLinkTupleIndices::COMPONENT_ID_INDEX>(*packet_with_metadata) = msg->compid;
        return packet_with_metadata;
    }
};

/// \brief Specialization for known compile-time Mavlink message without metadata, e.g. DataType == HEARTBEAT
template <typename DataType> struct SerializerParserHelper<DataType, MarshallingScheme::MAVLINK>
{
    static std::vector<char> serialize(const DataType& packet, int sysid = 1, int compid = 1)
    {
        return SerializerParserHelper<std::tuple<int, int, DataType>, MarshallingScheme::MAVLINK>::
            serialize(std::make_tuple(sysid, compid, packet));
    }

    static std::string type_name()
    {
        return SerializerParserHelper<std::tuple<int, int, DataType>,
                                      MarshallingScheme::MAVLINK>::type_name();
    }
    static std::string type_name(const DataType& /*d*/) { return type_name(); }

    template <typename CharIterator>
    static std::shared_ptr<DataType> parse(CharIterator bytes_begin, CharIterator bytes_end,
                                           CharIterator& actual_end,
                                           const std::string& /*type*/ = type_name())
    {
        auto packet_with_metadata =
            SerializerParserHelper<std::tuple<int, int, DataType>,
                                   MarshallingScheme::MAVLINK>::parse(bytes_begin, bytes_end,
                                                                      actual_end);
        return std::make_shared<DataType>(
            std::get<MAVLinkTupleIndices::PACKET_INDEX>(*packet_with_metadata));
    }
};

/// \brief Specialization for Mavlink with system id / component id metadata for tuple of <sysid, compid, msg>
template <typename Tuple,
          typename T = typename std::tuple_element<MAVLinkTupleIndices::PACKET_INDEX, Tuple>::type,
          typename std::enable_if<std::is_base_of<mavlink::Message, T>::value>::type* = nullptr>
constexpr int scheme()
{
    return goby::middleware::MarshallingScheme::MAVLINK;
}

/// \brief Specialization for Mavlink without metadata
template <typename T, typename std::enable_if<
                          std::is_base_of<mavlink::Message, T>::value ||
                          std::is_same<mavlink::mavlink_message_t, T>::value>::type* = nullptr>
constexpr int scheme()
{
    return goby::middleware::MarshallingScheme::MAVLINK;
}

} // namespace middleware
} // namespace goby

#endif
