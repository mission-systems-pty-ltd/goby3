// Copyright 2020-2021:
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

#ifndef GOBY_MOOS_FRONTSEAT_CONVERT_H
#define GOBY_MOOS_FRONTSEAT_CONVERT_H

class CMOOSCommClient;

namespace goby
{
namespace middleware
{
namespace frontseat
{
namespace protobuf
{
class NodeStatus;
} // namespace protobuf
} // namespace frontseat
} // namespace middleware

namespace moos
{
void convert_and_publish_node_status(
    const goby::middleware::frontseat::protobuf::NodeStatus& status, CMOOSCommClient& moos_comms);
}
} // namespace goby

#endif
