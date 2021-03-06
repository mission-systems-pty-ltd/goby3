// Copyright 2021:
//   GobySoft, LLC (2013-)
//   Community contributors (see AUTHORS file)
// File authors:
//   Toby Schneider <toby@gobysoft.org>
//
//
// This file is part of the Goby Underwater Autonomy Project Binaries
// ("The Goby Binaries").
//
// The Goby Binaries are free software: you can redistribute them and/or modify
// them under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// The Goby Binaries are distributed in the hope that they will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Goby.  If not, see <http://www.gnu.org/licenses/>.

#include <cassert>

#include "goby/util/debug_logger.h"
#include "goby/util/linebasedcomms.h"

using goby::glog;

int main(int /*argc*/, char* argv[])
{
    glog.add_stream(goby::util::logger::DEBUG3, &std::cerr);
    glog.set_name(argv[0]);

    goby::util::TCPServer server(64130);
    goby::util::TCPClient client("127.0.0.1", 64130);
    goby::util::TCPClient client2("127.0.0.1", 64130);

    server.start();
    client.start();
    client2.start();

    int count = 0;
    while (!(server.active() && client.active() && client2.active()) && count < 100)
    {
        glog.is_verbose() && glog << "Waiting for active" << std::endl;
        usleep(1e5);
        ++count;
    }

    assert(server.active() && client.active() && client2.active());

    glog.is_verbose() && glog << "Client local: " << client.local_endpoint()
                              << ", remote: " << client.remote_endpoint() << std::endl;

    glog.is_verbose() && glog << "Client2 local: " << client2.local_endpoint()
                              << ", remote: " << client2.remote_endpoint() << std::endl;

    assert(client.remote_endpoint() == "127.0.0.1:64130");
    assert(client2.remote_endpoint() == "127.0.0.1:64130");

    glog.is_verbose() && glog << "Server local: " << server.local_endpoint()
                              << ", remote connection count: " << server.remote_endpoints().size()
                              << std::endl;

    assert(server.local_endpoint() == "0.0.0.0:64130");
    assert(server.remote_endpoints().size() == 2);

    // client to server
    std::string client_endpoint;
    {
        std::string line;
        assert(!server.readline(&line));
        assert(!client.readline(&line));
        std::string test_string = "hello,world\r\n";
        client.write(test_string);
        sleep(1);
        goby::util::protobuf::Datagram d;
        bool data_was_read = server.readline(&d);
        assert(data_was_read);
        std::cout << "client to server:" << d.ShortDebugString() << std::endl;
        assert(d.data() == test_string);
        client_endpoint = d.src();
        assert(!server.readline(&line));
        assert(!client.readline(&line));
    }

    // client to server, no endpoint info
    {
        std::string line;
        assert(!server.readline(&line));
        assert(!client.readline(&line));
        std::string test_string = "hello,world!\r\n";
        client.write(test_string);
        sleep(1);
        std::string s;
        assert(server.readline(&s));
        assert(s == test_string);
        assert(!server.readline(&line));
        assert(!client.readline(&line));
    }

    glog.is_verbose() && glog << client_endpoint << std::endl;

    // server to one client
    {
        std::string line;
        std::string test_string2 = "hello,world2\r\n";
        goby::util::protobuf::Datagram d;
        d.set_data(test_string2);
        d.set_dest(client_endpoint);
        server.write(d);
        sleep(1);
        assert(client.readline(&line));
        assert(line == test_string2);
        assert(!client2.readline(&line));
    }

    // server to both clients
    {
        std::string line1, line2;
        std::string test_string3 = "hello,world3\r\n";
        server.write(test_string3);
        sleep(1);
        assert(client.readline(&line1));
        assert(client2.readline(&line2));
        assert(line1 == test_string3);
        assert(line2 == test_string3);
        assert(!server.readline(&line1));
        assert(!client.readline(&line1));
        assert(!client2.readline(&line1));
    }
}
