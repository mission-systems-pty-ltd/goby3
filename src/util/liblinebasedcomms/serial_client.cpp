// copyright 2009, 2010 t. schneider tes@mit.edu
// 
// this file is part of comms, a library for handling various communications.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this software.  If not, see <http://www.gnu.org/licenses/>.

#include <iostream>

#include <boost/thread.hpp>

#include "serial_client.h"


goby::util::SerialClient::SerialClient(const std::string& name,
                                       unsigned baud,
                                       const std::string& delimiter)
    : LineBasedClient<boost::asio::serial_port>(serial_port_, delimiter),
      serial_port_(io_service_),
      name_(name),
      baud_(baud)
{ }

bool goby::util::SerialClient::start_specific()
{
    try
    {
        serial_port_.open(name_);
    }
    catch(std::exception& e)
    {
        serial_port_.close();
        return false;
    }
    
    serial_port_.set_option(boost::asio::serial_port_base::baud_rate(baud_));

    // no flow control
    serial_port_.set_option(boost::asio::serial_port_base::flow_control(boost::asio::serial_port_base::flow_control::none));

    // 8N1
    serial_port_.set_option(boost::asio::serial_port_base::character_size(8));
    serial_port_.set_option(boost::asio::serial_port_base::parity(boost::asio::serial_port_base::parity::none));
    serial_port_.set_option(boost::asio::serial_port_base::stop_bits(boost::asio::serial_port_base::stop_bits::one));
    
    return true;    
}
