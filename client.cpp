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

struct client_op {
   std::string host {"127.0.0.1"};
   std::string port {"8080"};
   std::string sms;
   int users_size = 10;
   int handshake_tm_test_size = 10;
   int handshake_tm = 3;
   int handshake_tm_launch_interval = 100;
   int auth_timeout = 3;
   int sim_runs = 2;

   auto make_session_cf() const
   {
      return client_session_cf
      { {host} 
      , {port}
      , std::chrono::seconds {handshake_tm}
      , std::chrono::seconds {auth_timeout}};
   }
};

struct launcher_op {
   int begin;
   int end;
   std::chrono::milliseconds interval;
   std::string final_msg;
};

template <class T>
class test_launcher : 
   public std::enable_shared_from_this<test_launcher<T>> {
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
   test_launcher( boost::asio::io_context& ioc_
                , mgr_op_type mgr_op_
                , client_session_cf ccf_
                , launcher_op lop_)
   : ioc(ioc_)
   , mgr_op(mgr_op_)
   , ccf(ccf_)
   , lop(lop_)
   , timer(ioc)
   {}

   void run(boost::system::error_code ec)
   {
      if (ec)
         throw std::runtime_error("No error expected here.");

      if (lop.begin == lop.end) {
         std::cout << lop.final_msg << std::endl;
         return;
      }

      mgr_op.user = to_str(lop.begin, 6, 0);

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

void basic_tests(client_op const& op)
{
   boost::asio::io_context ioc;

   launcher_op lop1 { 0, op.handshake_tm_test_size
                    , std::chrono::milliseconds
                      {op.handshake_tm_launch_interval}
                    , {"Handshake test launch:       ok"}};

   auto ccf = op.make_session_cf();

   // Tests if the server times out connections that do not proceed
   // with the websocket handshake.
   std::make_shared< test_launcher<cmgr_handshake_tm>
                   >(ioc, cmgr_handshake_op {}, ccf, lop1)->run({});

   launcher_op lop2 { 0, op.handshake_tm_test_size
                    , std::chrono::milliseconds
                      {op.handshake_tm_launch_interval}
                    , {"After handshake test launch: ok"}};

   // Tests if the server drops connections that connect but do not
   // register or authenticate.
   std::make_shared< test_launcher<client_mgr_accept_timer>
                   >(ioc, cmgr_handshake_op {}, ccf, lop2)->run({});

   launcher_op lop3 { 0, op.users_size
                    , std::chrono::milliseconds
                      {op.handshake_tm_launch_interval}
                    , {"Login test with ret = -1:    ok"}};

   cmgr_login_cf cf2 { "" , "ok" , -1 };

   // Tests the sms timeout. Connections should be dropped if the
   // users tries to register but do not send the sms on time.
   // Connection is gracefully closed.
   std::make_shared< test_launcher<client_mgr_login>
                   >(ioc, cf2, ccf, lop3)->run({});

   launcher_op lop4 { op.users_size, 2 * op.users_size
                    , std::chrono::milliseconds
                      {op.handshake_tm_launch_interval}
                    , {"Login test with ret = -2:    ok"}};

   cmgr_login_cf cf4 { "" , "ok" , -2 };

   // Same as above but socket is shutdown.
   std::make_shared< test_launcher<client_mgr_login>
                   >(ioc, cf4, ccf, lop4)->run({});

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
   { {j1.dump()}
   , {j2.dump()}
   , {j3.dump()}
   };

   // Sends commands with typos and expects the server to not crash!
   for (auto const& cmd : cmds)
      std::make_shared<client_session<client_mgr_login_typo>
                      >( ioc
                       , op.make_session_cf()
                       , client_mgr_login_typo {cmd})->run();

   // Sends sms on time but the wrong one and expects the server to
   // release sessions correctly.
   for (auto i = 0; i < op.users_size; ++i)
         std::make_shared<client_session<client_mgr_sms>
                         >( ioc
                          , op.make_session_cf()
                          , client_mgr_sms
                            { to_str(i, 4, 0), "fail"
                            , "8r47"})->run();

   // Sends the correct sms on time.
   for (auto i = 0; i < op.users_size; ++i)
         std::make_shared<client_session<client_mgr_sms>
                         >( ioc
                          , op.make_session_cf()
                          , client_mgr_sms
                            { to_str(i, 4, 0), "ok"
                            , op.sms})->run();

   // TODO: Test this after implementing queries to the database.
   //basic_tests(op, "fail", 0, op.users_size, -1);
   //std::cout << "test_login_ok_4:    ok" << std::endl;

   auto const menu = gen_location_menu();
   auto const cmds2 = gen_create_groups(menu);

   std::make_shared<client_session<client_mgr_cg>
                   >( ioc
                    , op.make_session_cf()
                    , client_mgr_cg
                      {"ok", std::move(cmds2)})->run();
   ioc.run();
}

void test_simulation(client_op const& op, int begin, int end)
{
   auto const menu = gen_location_menu();
   auto const hashes = get_hashes(menu);

   boost::asio::io_context ioc;

   std::vector<std::shared_ptr<client_session<client_mgr_sim>>> sessions;
   for (auto i = begin; i < end; ++i) {
      auto tmp =
         std::make_shared<client_session<client_mgr_sim>
                         >( ioc
                          , op.make_session_cf()
                          , client_mgr_sim
                            {"ok", hashes, to_str(i, 4, 0)});
      tmp->run();
      sessions.push_back(tmp);
   }

   ioc.run();

   auto sent = 0;
   auto recv = 0;
   for (auto const& session : sessions) {
      sent += session->get_sent_msgs();
      recv += session->get_recv_msgs();
   }

   std::cout << "Sent:     " << sent << std::endl;
   std::cout << "Received: " << recv << std::endl;
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
         , "Server port.")
         ("ip,d"
         , po::value<std::string>(&op.host)->default_value("127.0.0.1")
         , "Server ip address.")
         ("users,u"
         , po::value<int>(&op.users_size)->default_value(10)
         , "Number of users.")

         ("handshake-test-size,s"
         , po::value<int>(&op.handshake_tm_test_size)->default_value(10)
         , "Number of handshake test clients. Also used for the "
           "after handshake timeout i.e. the time the client has "
           "the send the first command.")
         ("handshake-timeout,k"
         , po::value<int>(&op.handshake_tm)->default_value(3)
         , "Time before which the server should have given "
           "up on the handshake in seconds.")
         ("handshake-interval,g"
         , po::value<int>(&op.handshake_tm_launch_interval)->default_value(100)
         , "The interval with which we will lauch new handshake"
           " timeout test clients. Also used for the after handshake timeout.")

         ("sms,m"
         , po::value<std::string>(&op.sms)->default_value("8347")
         , "The code sent via email for account validation.")

         ("auth-timeout,l"
         , po::value<int>(&op.auth_timeout)->default_value(3)
         , "Time after before which the server should giveup witing for auth cmd.")

         ("simulations,r"
         , po::value<int>(&op.sim_runs)->default_value(2)
         , "Number of simulation runs.")
      ;

      po::variables_map vm;        
      po::store(po::parse_command_line(argc, argv, desc), vm);
      po::notify(vm);    

      if (vm.count("help")) {
         std::cout << desc << "\n";
         return 0;
      }

      std::cout << "==========================================" << std::endl;
      basic_tests(op);
      std::cout << "Basic tests:        ok" << std::endl;

      timer t;
      while (op.sim_runs != 0) {
         test_simulation(op, 0, op.users_size);
         std::cout << "Simulation run " << op.sim_runs <<
                   " completed." << std::endl;
         --op.sim_runs;
      }
      std::cout << "==========================================" << std::endl;

      return EXIT_SUCCESS;
   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
      return EXIT_FAILURE;
   }
}

