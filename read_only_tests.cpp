#include <stack>
#include <chrono>
#include <thread>
#include <memory>
#include <vector>
#include <string>
#include <utility>
#include <sstream>
#include <cstdlib>
#include <iostream>

#include <boost/asio/signal_set.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include "config.hpp"
#include "menu_parser.hpp"
#include "client_mgr_sms.hpp"
#include "client_session.hpp"
#include "client_mgr_register.hpp"
#include "session_launcher.hpp"
#include "client_mgr_accept_timer.hpp"

using namespace rt;

namespace po = boost::program_options;

struct options {
   std::string host {"127.0.0.1"};
   std::string port {"8080"};
   int initial_user = 0;
   int users_size = 10;
   int handshake_tm_test_size = 10;
   int handshake_tm = 3;
   int launch_interval = 100;
   int auth_timeout = 3;

   auto make_session_cf() const
   {
      return client_session_cf
      { {host} 
      , {port}
      , std::chrono::seconds {handshake_tm}
      , std::chrono::seconds {auth_timeout}
      };
   }

   auto make_handshake_laucher_op() const
   {
      return launcher_op
      { 0, handshake_tm_test_size
      , std::chrono::milliseconds {launch_interval}
      , {"Handshake test launch:         "}
      };
   }

   auto make_after_handshake_laucher_op() const
   {
      return launcher_op
      { 0, handshake_tm_test_size
      , std::chrono::milliseconds {launch_interval}
      , {"After handshake test launch:   "}
      };
   }

   auto make_sms_tm_laucher_op1() const
   {
      return launcher_op
      { initial_user
      , initial_user + users_size
      , std::chrono::milliseconds {launch_interval}
      , {"Register test with ret = -1:      "}};
   }

   auto make_sms_tm_laucher_op2() const
   {
      return launcher_op
      { initial_user
      , initial_user + users_size
      , std::chrono::milliseconds {launch_interval}
      , {"Register test with ret = -2:      "}};
   }

   auto make_sms_tm_laucher_op3() const
   {
      return launcher_op
      { initial_user
      , initial_user + users_size
      , std::chrono::milliseconds {launch_interval}
      , {"Register test with ret = -3:      "}};
   }

   auto make_wrong_sms_cf1() const
   {
      return launcher_op
      { initial_user + 3 * users_size
      , initial_user + 4 * users_size
      , std::chrono::milliseconds {launch_interval}
      , {"Wrong sms test with ret = -1:  "}
      };
   }

   auto make_wrong_sms_cf2() const
   {
      return launcher_op
      { initial_user + 4 * users_size
      , initial_user + 5 * users_size
      , std::chrono::milliseconds {launch_interval}
      , {"Wrong sms test with ret = -2:  "}
      };
   }

   auto make_wrong_sms_cf3() const
   {
      return launcher_op
      { initial_user + 5 * users_size
      , initial_user + 6 * users_size
      , std::chrono::milliseconds {launch_interval}
      , {"Wrong sms test with ret = -3:  "}
      };
   }
};

void read_only_tests(options const& op)
{
   boost::asio::io_context ioc;

   std::make_shared< session_launcher<cmgr_handshake_tm>
                   >( ioc
                    , cmgr_handshake_op {}
                    , op.make_session_cf()
                    , op.make_handshake_laucher_op()
                    )->run({});

   std::make_shared< session_launcher<client_mgr_accept_timer>
                   >( ioc
                    , cmgr_handshake_op {}
                    , op.make_session_cf()
                    , op.make_after_handshake_laucher_op()
                    )->run({});

   std::make_shared< session_launcher<client_mgr_register>
                   >( ioc
                    , cmgr_register_cf { "" , "ok" , -1}
                    , op.make_session_cf()
                    , op.make_sms_tm_laucher_op1()
                    )->run({});

   std::make_shared< session_launcher<client_mgr_register>
                   >( ioc
                    , cmgr_register_cf { "" , "ok" , -2}
                    , op.make_session_cf()
                    , op.make_sms_tm_laucher_op2()
                    )->run({});

   std::make_shared< session_launcher<client_mgr_register>
                   >( ioc
                    , cmgr_register_cf { "" , "ok" , -3}
                    , op.make_session_cf()
                    , op.make_sms_tm_laucher_op3()
                    )->run({});

   std::make_shared< session_launcher<client_mgr_sms>
                   >( ioc
                    , cmgr_sms_op {"", "fail", "8r47", -1}
                    , op.make_session_cf()
                    , op.make_wrong_sms_cf1()
                    )->run({});

   std::make_shared< session_launcher<client_mgr_sms>
                   >( ioc
                    , cmgr_sms_op {"", "fail", "8r47", -2}
                    , op.make_session_cf()
                    , op.make_wrong_sms_cf2()
                    )->run({});

   std::make_shared< session_launcher<client_mgr_sms>
                   >( ioc
                    , cmgr_sms_op {"", "fail", "8r47", -3}
                    , op.make_session_cf()
                    , op.make_wrong_sms_cf3()
                    )->run({});

   json j1;
   j1["cmd"] = "logrn";
   j1["tel"] = "aaaa";

   json j2;
   j2["crd"] = "register";
   j2["tel"] = "bbbb";

   json j3;
   j3["crd"] = "register";
   j3["Teal"] = "cccc";

   std::vector<std::string> cmds
   { {j1.dump()} , {j2.dump()} , {j3.dump()} };

   // Sends commands with typos and expects the server to not crash!
   for (auto const& cmd : cmds)
      std::make_shared<client_session<client_mgr_register_typo>
                      >( ioc
                       , op.make_session_cf()
                       , cmd)->run();

   ioc.run();
}

int main(int argc, char* argv[])
{
   try {
      options op;
      po::options_description desc("Options");
      desc.add_options()
         ("help,h", "produce help message")
         ( "port,p"
         , po::value<std::string>(&op.port)->default_value("8080")
         , "Server port."
         )
         ("ip,d"
         , po::value<std::string>(&op.host)->default_value("127.0.0.1")
         , "Server ip address."
         )

         ("initial-user"
         , po::value<int>(&op.initial_user)->default_value(0)
         , "Id of the first user."
         )
         ("users,u"
         , po::value<int>(&op.users_size)->default_value(10)
         , "Number of users."
         )

         ("launch-interval,g"
         , po::value<int>(&op.launch_interval)->default_value(100)
         , "Interval used to launch test clients."
         )

         ("handshake-test-size,s"
         , po::value<int>(&op.handshake_tm_test_size)->default_value(10)
         , "Number of handshake test clients. Also used for the "
           "after handshake timeout i.e. the time the client has "
           "the send the first command."
         )
         ("handshake-timeout,k"
         , po::value<int>(&op.handshake_tm)->default_value(3)
         , "Time before which the server should have given "
           "up on the handshake in seconds.")

         ("auth-timeout,l"
         , po::value<int>(&op.auth_timeout)->default_value(3)
         , "Time after before which the server should giveup witing for auth cmd.")
      ;

      po::variables_map vm;        
      po::store(po::parse_command_line(argc, argv, desc), vm);
      po::notify(vm);    

      if (vm.count("help")) {
         std::cout << desc << "\n";
         return 0;
      }

      read_only_tests(op);

      return 0;
   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
      return EXIT_FAILURE;
   }
}

