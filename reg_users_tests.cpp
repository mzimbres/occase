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
#include "client_mgr_sim.hpp"
#include "client_session.hpp"
#include "session_launcher.hpp"

using namespace rt;

namespace po = boost::program_options;

struct client_op {
   std::string host {"127.0.0.1"};
   std::string port {"8080"};
   std::string sms;
   int initial_user = 0;
   int users_size = 10;
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

   auto make_correct_sms_cf1() const
   {
      return launcher_op
      { initial_user
      , initial_user + users_size
      , std::chrono::milliseconds {launch_interval}
      , {"Correct sms test with ret = -1:"}
      };
   }

   auto make_correct_sms_cf2() const
   {
      return launcher_op
      { initial_user + users_size
      , initial_user + 2 * users_size
      , std::chrono::milliseconds {launch_interval}
      , {"Correct sms test with ret = -2:"}
      };
   }

   auto make_correct_sms_cf3() const
   {
      return launcher_op
      { initial_user + 2 * users_size
      , initial_user + 3 * users_size
      , std::chrono::milliseconds {launch_interval}
      , {"Correct sms test with ret = -3:"}
      };
   }
};

void reg_users_tests(client_op const& op)
{
   boost::asio::io_context ioc;

   std::make_shared< session_launcher<client_mgr_sms>
                   >( ioc
                    , cmgr_sms_op {"", "ok", op.sms, -1}
                    , op.make_session_cf()
                    , op.make_correct_sms_cf1()
                    )->run({});

   std::make_shared< session_launcher<client_mgr_sms>
                   >( ioc
                    , cmgr_sms_op {"", "ok", op.sms, -2}
                    , op.make_session_cf()
                    , op.make_correct_sms_cf2()
                    )->run({});

   std::make_shared< session_launcher<client_mgr_sms>
                   >( ioc
                    , cmgr_sms_op {"", "ok", op.sms, -3}
                    , op.make_session_cf()
                    , op.make_correct_sms_cf3()
                    )->run({});
   ioc.run();
}

int main(int argc, char* argv[])
{
   try {
      client_op op;
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

         ("handshake-timeout,k"
         , po::value<int>(&op.handshake_tm)->default_value(3)
         , "Time before which the server should have given "
           "up on the handshake in seconds.")

         ("sms,m"
         , po::value<std::string>(&op.sms)->default_value("8347")
         , "The code sent via email for account validation."
         )

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

      reg_users_tests(op);

      return 0;
   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
      return EXIT_FAILURE;
   }
}

