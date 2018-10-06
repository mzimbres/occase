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
#include "client_mgr_accept_timer.hpp"

//using work_type =
//   boost::asio::executor_work_guard<
//      boost::asio::io_context::executor_type>;
//
//work_type work;
//work.reset();
// work(boost::asio::make_work_guard(ioc))

namespace po = boost::program_options;

struct launcher_op {
   int begin;
   int end;
   std::chrono::milliseconds interval;
   std::string final_msg;
};

struct client_op {
   std::string host {"127.0.0.1"};
   std::string port {"8080"};
   std::string sms;
   int users_size = 10;
   int handshake_tm_test_size = 10;
   int handshake_tm = 3;
   int launch_interval = 100;
   int auth_timeout = 3;
   int sim_runs = 2;
   int number_of_groups;

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
      , {"Handshake test launch:        "}
      };
   }

   auto make_after_handshake_laucher_op() const
   {
      return launcher_op
      { 0, handshake_tm_test_size
      , std::chrono::milliseconds {launch_interval}
      , {"After handshake test launch:  "}
      };
   }

   auto make_sms_tm_laucher_op1() const
   {
      return launcher_op
      { 0, users_size
      , std::chrono::milliseconds {launch_interval}
      , {"Login test with ret = -1:     "}};
   }

   auto make_sms_tm_laucher_op2() const
   {
      return launcher_op
      { users_size, 2 * users_size
      , std::chrono::milliseconds {launch_interval}
      , {"Login test with ret = -2:     "}};
   }

   auto make_sms_tm_laucher_op3() const
   {
      return launcher_op
      { 2 * users_size, 3 * users_size
      , std::chrono::milliseconds {launch_interval}
      , {"Login test with ret = -3:     "}};
   }

   auto make_wrong_sms_cf1() const
   {
      return launcher_op
      { 3 * users_size, 4 * users_size
      , std::chrono::milliseconds {launch_interval}
      , {"Wrong sms test with ret = -1: "}
      };
   }

   auto make_wrong_sms_cf2() const
   {
      return launcher_op
      { 4 * users_size, 5 * users_size
      , std::chrono::milliseconds {launch_interval}
      , {"Wrong sms test with ret = -2: "}
      };
   }

   auto make_wrong_sms_cf3() const
   {
      return launcher_op
      { 5 * users_size, 6 * users_size
      , std::chrono::milliseconds {launch_interval}
      , {"Wrong sms test with ret = -3: "}
      };
   }

   auto make_correct_sms_cf1() const
   {
      return launcher_op
      { 6 * users_size, 7 * users_size
      , std::chrono::milliseconds {launch_interval}
      , {"Correct sms test with ret = -1:"}
      };
   }

   auto make_correct_sms_cf2() const
   {
      return launcher_op
      { 7 * users_size, 8 * users_size
      , std::chrono::milliseconds {launch_interval}
      , {"Correct sms test with ret = -2:"}
      };
   }

   auto make_correct_sms_cf3() const
   {
      return launcher_op
      { 8 * users_size, 9 * users_size
      , std::chrono::milliseconds {launch_interval}
      , {"Correct sms test with ret = -3:"}
      };
   }

   auto make_sim_cf() const
   {
      return launcher_op
      { 0, users_size
      , std::chrono::milliseconds {launch_interval}
      , {"Launch of sim clients:         "}
      };
   }
};

template <class T>
class session_launcher : 
   public std::enable_shared_from_this<session_launcher<T>> {
public:
   using mgr_type = T;
   using mgr_op_type = typename mgr_type::options_type;
   using client_type = client_session<mgr_type>;

private:
   boost::asio::io_context& ioc;
   mgr_op_type mgr_op;
   client_session_cf ccf;
   launcher_op lop;
   boost::asio::steady_timer timer;
 
public:
   session_launcher( boost::asio::io_context& ioc_
                   , mgr_op_type mgr_op_
                   , client_session_cf ccf_
                   , launcher_op lop_)
   : ioc(ioc_)
   , mgr_op(mgr_op_)
   , ccf(ccf_)
   , lop(lop_)
   , timer(ioc)
   {}

   ~session_launcher()
   {
      // TODO: Implement this.
      //std::vector<std::shared_ptr<client_session<client_mgr_sim>>> sessions;
      //auto sent = 0;
      //auto recv = 0;
      //for (auto const& session : sessions) {
      //   sent += session->get_sent_msgs();
      //   recv += session->get_recv_msgs();
      //}

      //std::cout << "Sent:     " << sent << std::endl;
      //std::cout << "Received: " << recv << std::endl;
   }

   void run(boost::system::error_code ec)
   {
      if (ec)
         throw std::runtime_error("No error expected here.");

      if (lop.begin == lop.end) {
         if (!std::empty(lop.final_msg))
            std::cout << lop.final_msg << " "
                      << lop.end << std::endl;
         return;
      }

      mgr_op.user = to_str(lop.begin);

      std::make_shared<client_type>( ioc
                                   , ccf
                                   , mgr_op)->run();

      timer.expires_after(lop.interval);

      auto handler = [p = this->shared_from_this()](auto ec)
      { p->run(ec); };

      timer.async_wait(handler);
      ++lop.begin;
   }
};

void basic_tests1(client_op const& op)
{
   boost::asio::io_context ioc;

   // Tests if the server times out connections that do not proceed
   // with the websocket handshake.
   std::make_shared< session_launcher<cmgr_handshake_tm>
                   >( ioc, cmgr_handshake_op {}
                    , op.make_session_cf()
                    , op.make_handshake_laucher_op()
                    )->run({});


   // Tests if the server drops connections that connect but do not
   // register or authenticate.
   std::make_shared< session_launcher<client_mgr_accept_timer>
                   >( ioc
                    , cmgr_handshake_op {}
                    , op.make_session_cf()
                    , op.make_after_handshake_laucher_op()
                    )->run({});

   ioc.run();
}

void basic_tests2(client_op const& op)
{
   boost::asio::io_context ioc;
   //
   // Tests the sms timeout. Connections should be dropped if the
   // users tries to register but do not send the sms on time.
   // Connection is gracefully closed.
   std::make_shared< session_launcher<client_mgr_login>
                   >( ioc
                    , cmgr_login_cf { "" , "ok" , -1}
                    , op.make_session_cf()
                    , op.make_sms_tm_laucher_op1()
                    )->run({});

   // Same as above but socket is shutdown and closed.
   //std::make_shared< session_launcher<client_mgr_login>
   //                >( ioc
   //                 , cmgr_login_cf { "" , "ok" , -2 }
   //                 , op.make_session_cf()
   //                 , op.make_sms_tm_laucher_op2()
   //                 )->run({});

   // Same as above but socket is only closed.
   // TODO: This in combination with the -1 version is resulting in a
   // very strange bug where the json is received only in part by the
   // create_group_ack further ahead. No idea what is happening.
   std::make_shared< session_launcher<client_mgr_login>
                   >( ioc
                    , cmgr_login_cf { "" , "ok" , -3 }
                    , op.make_session_cf()
                    , op.make_sms_tm_laucher_op3()
                    )->run({});

   ioc.run();
}

void basic_tests3(client_op const& op)
{
   boost::asio::io_context ioc;

   // Sends sms on time but the wrong one and expects the server to
   // release sessions correctly. Finishes with a close frame.
   std::make_shared< session_launcher<client_mgr_sms>
                   >( ioc
                    , cmgr_sms_op {"", "fail", "8r47", -1}
                    , op.make_session_cf()
                    , op.make_wrong_sms_cf1()
                    )->run({});

   // Same as above but shuts down the socket instead of gracefully
   // closing by sending a close frame.
   std::make_shared< session_launcher<client_mgr_sms>
                   >( ioc
                    , cmgr_sms_op {"", "fail", "8r47", -2}
                    , op.make_session_cf()
                    , op.make_wrong_sms_cf2()
                    )->run({});

   // Same as above but closes the socket instead of shuting it down.
   std::make_shared< session_launcher<client_mgr_sms>
                   >( ioc
                    , cmgr_sms_op {"", "fail", "8r47", -3}
                    , op.make_session_cf()
                    , op.make_wrong_sms_cf3()
                    )->run({});

   ioc.run();
}

void basic_tests4(client_op const& op)
{
   boost::asio::io_context ioc;

   // Sends the correct sms on time and closes gracefully.
   std::make_shared< session_launcher<client_mgr_sms>
                   >( ioc
                    , cmgr_sms_op {"", "ok", op.sms, -1}
                    , op.make_session_cf()
                    , op.make_correct_sms_cf1()
                    )->run({});

   // Same as above but shuts down the socket instead of sending a
   // close frame.
   std::make_shared< session_launcher<client_mgr_sms>
                   >( ioc
                    , cmgr_sms_op {"", "ok", op.sms, -2}
                    , op.make_session_cf()
                    , op.make_correct_sms_cf2()
                    )->run({});

   // Same as above but only closes the socket.
   std::make_shared< session_launcher<client_mgr_sms>
                   >( ioc
                    , cmgr_sms_op {"", "ok", op.sms, -3}
                    , op.make_session_cf()
                    , op.make_correct_sms_cf3()
                    )->run({});

   // TODO: Test if login with already registered user fails. Can be
   // implemented only after implementing database queries.

   ioc.run();
}

void basic_tests5(client_op const& op)
{
   boost::asio::io_context ioc;

   std::make_shared<client_session<client_mgr_cg>
                   >( ioc
                    , op.make_session_cf()
                    , client_mgr_cg::options_type
                      {"Marcelo1", "ok", op.number_of_groups}
                    )->run();

   ioc.run();
}

void basic_tests6(client_op const& op)
{
   boost::asio::io_context ioc;
   std::make_shared<client_session<client_mgr_cg>
                   >( ioc
                    , op.make_session_cf()
                    , client_mgr_cg::options_type
                      {"Marcelo2", "fail", op.number_of_groups}
                    )->run();
   ioc.run();
}

void basic_tests7(client_op const& op)
{
   boost::asio::io_context ioc;

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
                    , cmgr_sim_op {"", "ok", op.number_of_groups}
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

         ("number-of-groups,a"
         , po::value<int>(&op.number_of_groups)->default_value(20)
         , "Number of groups to generate."
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
      basic_tests1(op);
      basic_tests2(op);
      basic_tests3(op);
      basic_tests4(op);
      basic_tests5(op);
      basic_tests6(op);
      basic_tests7(op);
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

