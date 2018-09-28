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
   int users_size = 10;
   int conn_test_size = 10;
   long unsigned acc_test_size = 10;
   int handshake_timeout = 3;
   int auth_timeout = 3;

   auto session_config() const
   {
      return client_session_config
      { {host} 
      , {port}
      , std::chrono::seconds {handshake_timeout}
      , std::chrono::seconds {auth_timeout}};
   }
};

// Tests if the server sets a timeout after a connection.
class test_on_conn : public std::enable_shared_from_this<test_on_conn> {
public:
   using mgr_type = client_mgr_on_connect_timer;
   using client_type = client_session<mgr_type>;

private:
   boost::asio::io_context& ioc;
   std::chrono::milliseconds interval;
   int number_of_runs;
   client_session_config scf;
   boost::asio::steady_timer timer;
 
public:
   test_on_conn( boost::asio::io_context& ioc_
      , std::chrono::milliseconds interval_
      , int runs
      , client_session_config scf_)
   : ioc(ioc_)
   , interval(interval_) 
   , number_of_runs(runs)
   , scf(scf_)
   , timer(ioc)
   {}
      
   void run(boost::system::error_code ec)
   {
      if (ec)
         throw std::runtime_error("No error expected here.");

      if (number_of_runs-- == 0) {
         std::cout << "test_connect_timer: ok" << std::endl;
         return;
      }

      std::make_shared<client_type>(ioc, scf, mgr_type {})->run();

      timer.expires_after(interval);

      auto handler = [p = this->shared_from_this()](auto ec)
      { p->run(ec); };

      timer.async_wait(handler);
   }
};

void test_accept_timer(client_op const& op)
{
   using mgr_type = client_mgr_accept_timer;
   using client_type = client_session<mgr_type>;

   boost::asio::io_context ioc;

   std::make_shared<test_on_conn>( ioc 
                                 , std::chrono::milliseconds {100}
                                 , op.conn_test_size
                                 , op.session_config())->run({});

   std::vector<mgr_type> mgrs {op.acc_test_size};

   for (auto& mgr : mgrs)
      std::make_shared<client_type>(ioc, op.session_config(), mgr)->run();

   ioc.run();
}

void test_login( client_op const& op
               , char const* expected
               , int begin
               , int end
               , int ret)
{
   using mgr_type = client_mgr_login;
   using client_type = client_session<mgr_type>;

   boost::asio::io_context ioc;

   std::vector<mgr_type> mgrs;

   for (auto i = begin; i < end; ++i)
      mgrs.push_back({to_str(i, 4, 0), expected, ret});

   for (auto& mgr : mgrs)
      std::make_shared<client_type>(ioc, op.session_config(), mgr)->run();

   ioc.run();
}

void test_flood_login( client_op const& op
                     , int begin
                     , int end
                     , int more)
{
   using mgr_type = client_mgr_login;
   using client_type = client_session<mgr_type>;

   boost::asio::io_context ioc;

   std::vector<mgr_type> mgrs;

   for (auto i = begin; i < end; ++i)
      mgrs.push_back({to_str(i, 4, 0), "ok", -1});

   for (auto i = end; i < more; ++i)
      mgrs.push_back({to_str(i, 4, 0), "fail", -1});

   for (auto& mgr : mgrs)
      std::make_shared<client_type>(ioc, op.session_config(), mgr)->run();

   ioc.run();
}

void test_login_typo(client_op const& op)
{
   using mgr_type = client_mgr_login_typo;
   using client_type = client_session<mgr_type>;

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

   std::vector<mgr_type> mgrs
   { {j1.dump()}
   , {j2.dump()}
   , {j3.dump()}
   };

   for (auto& mgr : mgrs)
      std::make_shared<client_type>(ioc, op.session_config(), mgr)->run();

   ioc.run();
}

auto test_sms( client_op const& op
             , std::string const& expected
             , std::string const& sms)
{
   using mgr_type = client_mgr_sms;
   using client_type = client_session<mgr_type>;

   boost::asio::io_context ioc;

   std::vector<mgr_type> mgrs;

   for (auto i = 0; i < op.users_size; ++i)
      mgrs.push_back({to_str(i, 4, 0), expected, sms});

   std::vector<std::shared_ptr<client_type>> sessions;
   for (auto& mgr : mgrs) {
      auto tmp =
         std::make_shared<client_type>(ioc, op.session_config(), mgr);
      sessions.push_back(tmp);
   }

   for (auto& session : sessions)
      session->run();

   ioc.run();

   std::vector<user_bind> binds;
   for (auto const& session : sessions)
      if (session->get_mgr().bind.index != -1)
         binds.push_back(session->get_mgr().bind);

   return binds;
}

auto test_auth(client_op const& op, std::vector<user_bind> binds)
{
   using mgr_type = client_mgr_auth;
   using client_type = client_session<mgr_type>;

   boost::asio::io_context ioc;

   std::vector<mgr_type> mgrs;
   for (auto& bind : binds)
      mgrs.emplace_back(bind, "ok");

   std::vector<std::shared_ptr<client_type>> sessions;
   for (auto& mgr : mgrs) {
      auto tmp =
         std::make_shared<client_type>(ioc, op.session_config(), mgr);
      sessions.push_back(tmp);
   }

   for (auto& session : sessions)
      session->run();

   ioc.run();
}

auto test_create_group(client_op const& op, user_bind bind)
{
   using mgr_type = client_mgr_cg;
   using client_type = client_session<mgr_type>;

   auto menu = gen_location_menu();
   auto hash_patches = gen_hash_patches(menu);
   menu = menu.patch(hash_patches);
   auto const tmp = gen_create_groups(menu, bind);
   std::stack<std::string> cmds;
   for (auto const& o : tmp)
      cmds.push(o);

   mgr_type mgr {"ok", std::move(cmds), bind};

   boost::asio::io_context ioc;
   std::make_shared<client_type>(ioc, op.session_config(), mgr)->run();
   ioc.run();
}

void test_simulation(client_op const& op, std::vector<user_bind> binds)
{
   //std::cout << "Binds size: " << std::size(binds) << std::endl;
   using mgr_type = client_mgr_sim;
   using client_type = client_session<mgr_type>;

   auto menu = gen_location_menu();
   auto hash_patches = gen_hash_patches(menu);
   menu = menu.patch(hash_patches);

   auto const hashes = get_hashes(menu);
   std::stack<std::string> hashes_st;
   for (auto const& o : hashes)
      hashes_st.push(o);

   std::vector<mgr_type> mgrs;
   for (auto const& bind : binds) {
      auto const tmp = gen_join_groups(menu, bind);
      std::stack<std::string> cmds;
      for (auto const& o : tmp)
         cmds.push(o);

      mgrs.emplace_back("ok", cmds, hashes_st, bind);
   }

   boost::asio::io_context ioc;

   std::vector<std::shared_ptr<client_type>> sessions;
   for (auto& mgr : mgrs) {
      auto tmp =
         std::make_shared<client_type>(ioc, op.session_config(), mgr);
      sessions.push_back(tmp);
   }

   for (auto& session : sessions)
      session->run();

   ioc.run();

   int sent = 0;
   int recv = 0;
   for (auto& session : sessions) {
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
         ("handshake-size,s"
         , po::value<int>(&op.conn_test_size)->default_value(10)
         , "Number of handshake test clients.")
         ("accept-size,a"
         , po::value<long unsigned>(&op.acc_test_size)->default_value(10)
         , "Number of after accept test clients.")
         ("handshake-timeout,k"
         , po::value<int>(&op.handshake_timeout)->default_value(3)
         , "Time after before which the server should giveup the handshake.")
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

      std::cout << "==========================================" << std::endl;

      // Tests if the server drops connections that connect by do not
      // register or authenticate.
      test_accept_timer(op);
      std::cout << "test_accept_timer:  ok" << std::endl;

      // Tests the sms timeout. Connections should be dropped if the
      // users tries to register but do not send the sms on time.
      // Connection is gracefully closed.
      test_login(op, "ok", 0, op.users_size, -1);
      std::cout << "test_login_ok_1:    ok" << std::endl;

      // Tests if the previous login commands were all released. That
      // means if we send again users_size registrations there should
      // be enough indexes for them.
      // Connection is shutdown so that the server does not notice it
      // imediatelly. The next test should fail if we do not wait
      // until the sessions begin to timeout in the server.
      test_login(op, "ok", 0, op.users_size, -2);
      std::cout << "test_login_ok_2:    ok" << std::endl;

      std::this_thread::sleep_for(std::chrono::seconds {op.auth_timeout});

      // Tests if the previous login commands were all released. That
      // means if we send again users_size registrations there should
      // be enough indexes for them.
      // Connection is gracefully closed.
      test_login(op, "ok", 0, op.users_size, -1);
      std::cout << "test_login_ok_21:   ok" << std::endl;

      std::cout << "test_login_ok_2:    ok" << std::endl;
      // Sends more logins than the server has available user entries.
      // Assumes all messages will arrive in the server before the
      // first one begins to timeout.
      test_flood_login(op, 0, op.users_size, 50);
      std::cout << "test_flood_login:   ok" << std::endl;

      // Sends commands with typos.
      test_login_typo(op);
      std::cout << "test_login_typo:    ok" << std::endl;

      // Sends sms on time but the wrong one and expects the server to
      // release indexes correctly.
      auto binds = test_sms(op, "fail", "8r47");
      if (!std::empty(binds)) {
         std::cerr << "Error: Binds array not empty." << std::endl;
         return EXIT_FAILURE;
      }
      std::cout << "test_wrong_sms:     ok" << std::endl;

      // Sends correct sms on time.
      binds = test_sms(op, "ok", "8347");
      if (std::empty(binds)) {
         std::cerr << "Error: Binds array empty." << std::endl;
         return 1;
      }
      std::cout << "test_correct_sms:   ok" << std::endl;

      // Test if the server refuses all logins after we occupied all
      // indexes in the last sms test. First using non-existing users.
      test_login(op, "fail", op.users_size, 2 * op.users_size, -1);
      std::cout << "test_login_ok_3:    ok" << std::endl;

      // Same as above but for already registered users.
      test_login(op, "fail", 0, op.users_size, -1);
      std::cout << "test_login_ok_4:    ok" << std::endl;

      // Test authentication with binds obtained in the sms step.
      test_auth(op, binds);
      std::cout << "test_auth:          ok" << std::endl;

      test_create_group(op, binds.front());
      std::cout << "test_create_group:  ok" << std::endl;

      timer t;
      test_simulation(op, binds);
      std::cout << "test_simulation:    ok" << std::endl;
      std::cout << "==========================================" << std::endl;

      return EXIT_SUCCESS;
   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
      return EXIT_FAILURE;
   }
}

