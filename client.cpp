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
#include "client_mgr_gmsg_check.hpp"
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
   int sim_runs = 2;
   int msgs_per_group;

   auto make_session_cf() const
   {
      return client_session_cf
      { {host} 
      , {port}
      , std::chrono::seconds {handshake_tm}
      , std::chrono::seconds {auth_timeout}
      };
   }

   auto make_sim_cf() const
   {
      return launcher_op
      { initial_user, initial_user + 1 * users_size
      , std::chrono::milliseconds {launch_interval}
      , {"Launch of sim clients:         "}
      };
   }

   auto make_gmsg_check_cf() const
   {
      return launcher_op
      { initial_user + 1 * users_size
      , initial_user + 3 * users_size
      , std::chrono::milliseconds {launch_interval}
      , {"Launch of sim clients:         "}
      };
   }
};

class timer {
private:
  std::chrono::time_point<std::chrono::system_clock> m_start;
public:
  timer() : m_start(std::chrono::system_clock::now()) {}
  auto get_count() const
  { 
    auto const end = std::chrono::system_clock::now();
    auto diff = end - m_start;
    auto diff2 = std::chrono::duration_cast<std::chrono::seconds>(diff);
    return diff2.count();
  }
  ~timer()
  {
    std::cout << "Time elapsed: " << get_count() << "s" << std::endl;
  }
};

void test_simulation(client_op const& op)
{
   boost::asio::io_context ioc;

   auto const sim_op =  op.make_sim_cf();

   auto const next = [&ioc, &op, &sim_op]()
   {
      timer t;
      auto const s2 = std::make_shared< session_launcher<client_mgr_sim>
                      >( ioc
                       , cmgr_sim_op
                         { "", "ok", op.msgs_per_group}
                       , op.make_session_cf()
                       , sim_op
                       );
      s2->run({});
   };

   auto const s = std::make_shared< session_launcher<client_mgr_gmsg_check>
                   >( ioc
                    , cmgr_gmsg_check_op
                      {"", sim_op.end - sim_op.begin
                      , op.msgs_per_group}
                    , op.make_session_cf()
                    , op.make_gmsg_check_cf()
                    );
   s->set_call(next);
   s->run({});

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

         ("simulations,r"
         , po::value<int>(&op.sim_runs)->default_value(2)
         , "Number of simulation runs."
         )

         ("msgs-per-group,b"
         , po::value<int>(&op.msgs_per_group)->default_value(3)
         , "Number of messages per group used in the simulation."
         )
      ;

      po::variables_map vm;        
      po::store(po::parse_command_line(argc, argv, desc), vm);
      po::notify(vm);    

      if (vm.count("help")) {
         std::cout << desc << "\n";
         return 0;
      }

      while (op.sim_runs != 0) {
         test_simulation(op);
         std::cout
            << "===========================================================> "
            << op.sim_runs << std::endl;
         --op.sim_runs;
      }

      return 0;
   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
      return EXIT_FAILURE;
   }
}

