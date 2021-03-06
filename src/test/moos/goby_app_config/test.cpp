// Copyright 2018-2021:
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

#include "goby/moos/goby_moos_app.h"
#include "goby/test/moos/goby_app_config/config.pb.h"

namespace goby
{
namespace test
{
namespace moos
{
class GobyMOOSAppTest : public goby::moos::GobyMOOSApp
{
  public:
    static GobyMOOSAppTest* get_instance();

  private:
    GobyMOOSAppTest(goby::test::moos::protobuf::AppConfig& cfg) : goby::moos::GobyMOOSApp(&cfg)
    {
        assert(!cfg.has_submessage());
        RequestQuit();
        std::cout << "All tests passed. " << std::endl;
    }

    ~GobyMOOSAppTest() override = default;
    void loop() override {}
    static GobyMOOSAppTest* inst_;
};
} // namespace moos
} // namespace test
} // namespace goby

std::shared_ptr<goby::test::moos::protobuf::AppConfig> master_config;
goby::test::moos::GobyMOOSAppTest* goby::test::moos::GobyMOOSAppTest::inst_ = nullptr;

goby::test::moos::GobyMOOSAppTest* goby::test::moos::GobyMOOSAppTest::get_instance()
{
    if (!inst_)
    {
        master_config.reset(new goby::test::moos::protobuf::AppConfig);
        inst_ = new goby::test::moos::GobyMOOSAppTest(*master_config);
    }
    return inst_;
}

int main(int argc, char* argv[])
{
    return goby::moos::run<goby::test::moos::GobyMOOSAppTest>(argc, argv);
}
