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

#include "utils.hpp"
#include "config.hpp"
#include "menu.hpp"
#include "client_mgr_confirm_code.hpp"
#include "client_mgr_pub.hpp"
#include "client_mgr_gmsg_check.hpp"
#include "client_session.hpp"
#include "session_launcher.hpp"

using namespace rt;

namespace po = boost::program_options;

struct client_op {
   std::string host {"127.0.0.1"};
   std::string port {"8080"};
   std::string code;
   int listen_users = 10;
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

   auto make_pub_cf() const
   {
      return launcher_cf
      { 0, listen_users
      , std::chrono::milliseconds {launch_interval}
      , {""}
      };
   }

   auto make_gmsg_check_cf() const
   {
      return launcher_cf
      { 0, listen_users
      , std::chrono::milliseconds {launch_interval}
      , {""}
      };
   }
};

void launch_sessions(client_op const& op)
{
   boost::asio::io_context ioc;

   auto const s =
      std::make_shared< session_launcher<client_mgr_gmsg_check>
                      >( ioc
                       , cmgr_gmsg_check_op {"", 100 , 100}
                       , op.make_session_cf()
                       , op.make_gmsg_check_cf()
                       );
   
   s->run({});
   ioc.run();
}

int main(int argc, char* argv[])
{
   try {
      client_op op;
      po::options_description desc("Options");
      desc.add_options()
         ("help,h", "App simulator.")
         ( "port,p"
         , po::value<std::string>(&op.port)->default_value("8080")
         , "Server port."
         )
         ("ip,d"
         , po::value<std::string>(&op.host)->default_value("127.0.0.1")
         , "Server ip address."
         )

         ("listen-users,c"
         , po::value<int>(&op.listen_users)->default_value(10)
         , "Number of listen users."
         )

         ("launch-interval,g"
         , po::value<int>(&op.launch_interval)->default_value(100)
         , "Interval used to launch test clients."
         )

         ("handshake-timeout,k"
         , po::value<int>(&op.handshake_tm)->default_value(3)
         , "Time before which the server should have given "
           "up on the handshake in seconds.")

         ("code,m"
         , po::value<std::string>(&op.code)->default_value("8347")
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

      set_fd_limits(500000);

      launch_sessions(op);

      return 0;
   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
      return EXIT_FAILURE;
   }
}

