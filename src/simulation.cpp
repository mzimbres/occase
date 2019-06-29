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
#include "test_clients.hpp"
#include "client_session.hpp"
#include "session_launcher.hpp"

using namespace rt;
using namespace rt::cli;

namespace po = boost::program_options;

struct client_op {
   std::string host {"127.0.0.1"};
   std::string port {"8080"};
   int listen_users = 10;
   int n_replies = 2;
   int handshake_tm = 3;
   int launch_interval = 100;
   int auth_timeout = 3;

   auto make_session_cfg() const
   {
      return session_shell_cfg
      { {host} 
      , {port}
      , std::chrono::seconds {handshake_tm}
      , std::chrono::seconds {auth_timeout}
      };
   }

   auto make_client_cfg(std::vector<login> logins) const
   {
      return launcher_cfg
      { logins
      , std::chrono::milliseconds {launch_interval}
      , {""}
      };
   }
};

void launch_sessions(client_op const& op, std::vector<login> logins)
{
   using client_type = simulator;
   using config_type = client_type::options_type;

   boost::asio::io_context ioc;

   auto const s =
      std::make_shared< session_launcher<client_type>
                      >( ioc
                       , config_type {{}, op.n_replies}
                       , op.make_session_cfg()
                       , op.make_client_cfg(logins)
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

         ("number-of-replies,r"
         , po::value<int>(&op.n_replies)->default_value(10)
         , "The number of replies."
         )

         ("launch-interval,g"
         , po::value<int>(&op.launch_interval)->default_value(100)
         , "Interval used to launch test clients."
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

      set_fd_limits(500000);

      auto logins = test_reg(op.make_session_cfg(), op.listen_users);
      launch_sessions(op, std::move(logins));

      return 0;
   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
      return EXIT_FAILURE;
   }
}

