// Copyright 2016-2021:
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

#include "goby/middleware/marshalling/protobuf.h"
#include "goby/time.h"
#include "goby/time/io.h"
#include "goby/zeromq/application/multi_thread.h"

#include <boost/units/io.hpp>
#include <memory>

#include <sys/types.h>
#include <sys/wait.h>

#include "goby/test/zeromq/multi_thread_app1/test.pb.h"
using goby::glog;

using namespace goby::test::zeromq::protobuf;

extern constexpr goby::middleware::Group widget1{"widget1", 1};
extern constexpr goby::middleware::Group widget2{"widget2"};
extern constexpr goby::middleware::Group ready{"ready"};

const std::string platform_name{"multi_thread_app1"};

constexpr int num_messages{10};

using AppBase = goby::zeromq::MultiThreadApplication<TestConfig>;

namespace goby
{
namespace test
{
namespace zeromq
{
void noop_func(const Widget& widget) {}

class TestRxConfigurator : public goby::middleware::ProtobufConfigurator<TestConfig>
{
  public:
    TestRxConfigurator(int argc, char* argv[])
        : goby::middleware::ProtobufConfigurator<TestConfig>(argc, argv)
    {
        TestConfig& cfg = mutable_cfg();
        cfg.mutable_app()->set_name("TestAppRx");
        cfg.mutable_interprocess()->set_platform(platform_name);
    }
};

class TestTxConfigurator : public goby::middleware::ProtobufConfigurator<TestConfig>
{
  public:
    TestTxConfigurator(int argc, char* argv[])
        : goby::middleware::ProtobufConfigurator<TestConfig>(argc, argv)
    {
        TestConfig& cfg = mutable_cfg();
        cfg.mutable_app()->set_name("TestAppTx");
        cfg.mutable_interprocess()->set_platform(platform_name);
    }
};

class TestThreadRx : public goby::middleware::SimpleThread<TestConfig>
{
  public:
    TestThreadRx(const TestConfig& cfg) : SimpleThread(cfg)
    {
        glog.is_verbose() && glog << "Rx Thread: pid: " << getpid()
                                  << ", thread: " << std::this_thread::get_id() << std::endl;

        glog.is_verbose() && glog << std::this_thread::get_id() << std::endl;

        interprocess().subscribe<widget1, Widget>([this](const Widget& w) { post(w); });
        interprocess().subscribe<widget2, Widget>([this](const Widget& w) { post(w); });
        ready = true;
    }

    ~TestThreadRx() override = default;

    void post(const Widget& widget)
    {
        glog.is_verbose() && glog << "Thread Rx: " << widget.DebugString() << std::flush;
        assert(widget.b() == rx_count_);
        ++rx_count_;

        interthread().publish<widget2>(widget);
    }

    static std::atomic<bool> ready;

  private:
    int rx_count_{0};
};

std::atomic<bool> TestThreadRx::ready{false};

class TestAppRx : public AppBase
{
  public:
    TestAppRx() : AppBase(10)
    {
        glog.is_verbose() && glog << "Rx App: pid: " << getpid()
                                  << ", thread: " << std::this_thread::get_id() << std::endl;
        interprocess().subscribe<widget1, Widget>([this](const Widget& w) { post(w); });
        interprocess().subscribe<widget2, Widget>([this](const Widget& w) { post2(w); });
        launch_thread<TestThreadRx>();

        while (!TestThreadRx::ready) usleep(1e4);

        interprocess().ready();
    }

    void loop() override {}

    void post(const Widget& widget)
    {
        glog.is_verbose() && glog << "App Rx: " << widget.DebugString() << std::flush;
        assert(widget.b() == rx_count_);
        ++rx_count_;
        if (rx_count_ == num_messages)
            quit();
    }

    void post2(const Widget& widget)
    {
        glog.is_verbose() && glog << "App Rx2: " << widget.DebugString() << std::flush;
    }

  private:
    int rx_count_{0};
};

class TestAppTx : public AppBase
{
  public:
    TestAppTx() : AppBase(100)
    {
        glog.is_verbose() && glog << "Tx App: pid: " << getpid()
                                  << ", thread: " << std::this_thread::get_id() << std::endl;

        // test subscribe interface options
        interthread().subscribe<widget1, Widget>([this](const Widget& w) { noop(w); });
        interthread().subscribe<widget1>([this](const Widget& w) { noop(w); });
        interthread().subscribe<widget1, Widget>(
            std::bind(&TestAppTx::noop, this, std::placeholders::_1));
        //        interthread().subscribe2<widget1>(
        // std::bind(&TestAppTx::noop, this, std::placeholders::_1));
        std::function<void(const Widget& widget)> f(
            std::bind(&TestAppTx::noop, this, std::placeholders::_1));
        interthread().subscribe<widget1>(f);
        interthread().subscribe<widget1, Widget>(&noop_func);
        interthread().subscribe<widget1>(&noop_func);

        interprocess().ready();
    }

    void loop() override
    {
        static int i = 0;
        ++i;

        if (!interprocess().hold_state())
        {
            glog.is_verbose() && glog << goby::time::SystemClock::now() << std::endl;
            Widget w;
            w.set_b(tx_count_++);
            {
                glog.is_verbose() && glog << "Tx: " << w.DebugString() << std::flush;
            }

            interprocess().publish<widget1>(w);

            if (tx_count_ == (num_messages + 5))
                quit();
        }
    }

    void noop(const Widget& widget) {}

  private:
    int tx_count_{0};
};
} // namespace zeromq
} // namespace test
} // namespace goby

int main(int argc, char* argv[])
{
    int child_pid = fork();

    std::unique_ptr<std::thread> t2, t3;
    std::unique_ptr<zmq::context_t> manager_context;
    std::unique_ptr<zmq::context_t> router_context;

    //    goby::glog.add_stream(goby::util::logger::DEBUG3, &std::cerr);

    if (child_pid != 0)
    {
        goby::zeromq::protobuf::InterProcessPortalConfig cfg;
        cfg.set_platform(platform_name);
        goby::zeromq::protobuf::InterProcessManagerHold hold;
        hold.add_required_client("TestAppRx");
        hold.add_required_client("TestAppTx");

        manager_context = std::make_unique<zmq::context_t>(1);
        router_context = std::make_unique<zmq::context_t>(1);
        goby::zeromq::Router router(*router_context, cfg);
        t2 = std::make_unique<std::thread>([&] { router.run(); });
        goby::zeromq::Manager manager(*manager_context, cfg, router, hold);
        t3 = std::make_unique<std::thread>([&] { manager.run(); });
        int wstatus;
        wait(&wstatus);
        router_context.reset();
        manager_context.reset();
        t2->join();
        t3->join();
        if (wstatus != 0)
            exit(EXIT_FAILURE);
    }
    else
    {
        // let manager and router start up
        // sleep(1);
        int child2_pid = fork();
        if (child2_pid != 0)
        {
            int wstatus;
            int rc = goby::run<goby::test::zeromq::TestAppRx>(
                goby::test::zeromq::TestRxConfigurator(argc, argv));
            wait(&wstatus);
            if (wstatus != 0)
                exit(EXIT_FAILURE);
            return rc;
        }
        else
        {
            return goby::run<goby::test::zeromq::TestAppTx>(
                goby::test::zeromq::TestTxConfigurator(argc, argv));
        }
    }
    std::cout << "All tests passed." << std::endl;
}
