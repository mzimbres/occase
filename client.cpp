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
#include "client_mgr_cg.hpp"
#include "client_mgr_sms.hpp"
#include "client_mgr_sim.hpp"
#include "client_session.hpp"
#include "client_mgr_login.hpp"
#include "session_launcher.hpp"
#include "client_mgr_accept_timer.hpp"

namespace po = boost::program_options;

struct client_op {
   std::string host {"127.0.0.1"};
   std::string port {"8080"};
   std::string sms;
   int initial_user = 0;
   int users_size = 10;
   int handshake_tm_test_size = 10;
   int handshake_tm = 3;
   int launch_interval = 100;
   int auth_timeout = 3;
   int sim_runs = 2;
   int group_begin;
   int number_of_groups;
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
      , {"Login test with ret = -1:      "}};
   }

   auto make_sms_tm_laucher_op2() const
   {
      return launcher_op
      { initial_user
      , initial_user + users_size
      , std::chrono::milliseconds {launch_interval}
      , {"Login test with ret = -2:      "}};
   }

   auto make_sms_tm_laucher_op3() const
   {
      return launcher_op
      { initial_user
      , initial_user + users_size
      , std::chrono::milliseconds {launch_interval}
      , {"Login test with ret = -3:      "}};
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

   auto make_sim_cf() const
   {
      return launcher_op
      { initial_user, initial_user + 3 * users_size
      , std::chrono::milliseconds {launch_interval}
      , {"Launch of sim clients:         "}
      };
   }
};

void basic_tests6(client_op const& op)
{
   boost::asio::io_context ioc;
   std::make_shared<client_session<client_mgr_cg>
                   >( ioc
                    , op.make_session_cf()
                    , client_mgr_cg::options_type
                      { "Marcelo2"
                      , "fail"
                      , op.group_begin
                      , op.number_of_groups}
                    )->run();
   ioc.run();
}

// Run tests while registering users and creating groups.
void prepare_server(client_op const& op)
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

   std::make_shared< session_launcher<client_mgr_login>
                   >( ioc
                    , cmgr_login_cf { "" , "ok" , -1}
                    , op.make_session_cf()
                    , op.make_sms_tm_laucher_op1()
                    )->run({});

   std::make_shared< session_launcher<client_mgr_login>
                   >( ioc
                    , cmgr_login_cf { "" , "ok" , -2}
                    , op.make_session_cf()
                    , op.make_sms_tm_laucher_op2()
                    )->run({});

   std::make_shared< session_launcher<client_mgr_login>
                   >( ioc
                    , cmgr_login_cf { "" , "ok" , -3}
                    , op.make_session_cf()
                    , op.make_sms_tm_laucher_op3()
                    )->run({});

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

   std::make_shared<client_session<client_mgr_cg>
                   >( ioc
                    , op.make_session_cf()
                    , client_mgr_cg::options_type
                      { "Marcelo1"
                      , "ok"
                      , op.group_begin
                      , op.number_of_groups}
                    )->run();

   json j1;
   j1["cmd"] = "logrn";
   j1["tel"] = "aaaa";

   json j2;
   j2["crd"] = "login";
   j2["tel"] = "bbbb";

   json j3;
   j3["crd"] = "login";
   j3["Teal"] = "cccc";

   std::vector<std::string> cmds
   { {j1.dump()} , {j2.dump()} , {j3.dump()} };

   // Sends commands with typos and expects the server to not crash!
   for (auto const& cmd : cmds)
      std::make_shared<client_session<client_mgr_login_typo>
                      >( ioc
                       , op.make_session_cf()
                       , cmd)->run();

   ioc.run();
}

void test_simulation(client_op const& op)
{
   boost::asio::io_context ioc;

   std::make_shared< session_launcher<client_mgr_sim>
                   >( ioc
                    , cmgr_sim_op
                      { "", "ok", op.number_of_groups
                      , op.msgs_per_group}
                    , op.make_session_cf()
                    , op.make_sim_cf()
                    )->run({});

   ioc.run();
}

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

         ("group-begin"
         , po::value<int>(&op.group_begin)->default_value(0)
         , "Code of the first group."
         )
         ("number-of-groups,a"
         , po::value<int>(&op.number_of_groups)->default_value(20)
         , "Number of groups to generate."
         )

         ("msgs-per-group,b"
         , po::value<int>(&op.msgs_per_group)->default_value(20)
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

      std::cout << "==========================================" << std::endl;
      prepare_server(op);
      basic_tests6(op);
      std::cout << "Basic tests:        ok" << std::endl;

      timer t;
      while (op.sim_runs != 0) {
         test_simulation(op);
         std::cout << "Simulation run " << op.sim_runs
                   << " completed." << std::endl;
         --op.sim_runs;
      }
      std::cout << "==========================================" << std::endl;

      return EXIT_SUCCESS;
   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
      return EXIT_FAILURE;
   }
}

