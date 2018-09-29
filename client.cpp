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
   int handshake_tm_test_size = 10;
   int handshake_tm = 3;
   int handshake_tm_launch_interval = 100;
   int auth_timeout = 3;

   auto session_config() const
   {
      return client_session_config
      { {host} 
      , {port}
      , std::chrono::seconds {handshake_tm}
      , std::chrono::seconds {auth_timeout}};
   }
};

template <class T>
class handshake_test_launcher : 
   public std::enable_shared_from_this<handshake_test_launcher<T>> {
public:
   using mgr_type = T;
   using client_type = client_session<mgr_type>;

private:
   boost::asio::io_context& ioc;
   client_op op;
   std::chrono::milliseconds interval;
   boost::asio::steady_timer timer;
 
public:
   handshake_test_launcher( boost::asio::io_context& ioc_
                          , client_op const& op_)
   : ioc(ioc_)
   , op(op_)
   , interval(op.handshake_tm_launch_interval)
   , timer(ioc)
   {}
      
   void run(boost::system::error_code ec)
   {
      if (ec)
         throw std::runtime_error("No error expected here.");

      if (op.handshake_tm_test_size-- == 0) {
         std::cout << "Handshake/Auth test: ok" << std::endl;
         return;
      }

      std::make_shared<client_type>( ioc, op.session_config()
                                   , mgr_type {})->run();

      timer.expires_after(interval);

      auto handler = [p = this->shared_from_this()](auto ec)
      { p->run(ec); };

      timer.async_wait(handler);
   }
};

class test_login_launcher : 
   public std::enable_shared_from_this<test_login_launcher> {
public:
   using mgr_type = client_mgr_login;
   using client_type = client_session<mgr_type>;

private:
   boost::asio::io_context& ioc;
   client_op op;
   std::chrono::milliseconds interval {100};
   boost::asio::steady_timer timer;
   int begin = 0;
   int end = 0;
   std::string expected;
   int ret;
 
public:
   test_login_launcher( boost::asio::io_context& ioc_
                      , client_op const& op_
                      , int b_
                      , int e_
                      , std::string expected_
                      , int ret_)
   : ioc(ioc_)
   , op(op_)
   , timer(ioc)
   , begin(b_)
   , end(e_)
   , expected(expected_)
   , ret(ret_)
   {}
      
   void run(boost::system::error_code ec)
   {
      if (ec)
         throw std::runtime_error("No error expected here.");

      if (begin == end) {
         std::cout << "Test login timeout: ok" << std::endl;
         return;
      }

      mgr_type mgr {to_str(begin, 4, 0), expected, ret};

      std::make_shared<client_type>( ioc, op.session_config()
                                   , mgr)->run();

      timer.expires_after(interval);

      auto handler = [p = this->shared_from_this()](auto ec)
      { p->run(ec); };

      timer.async_wait(handler);
      begin++;
   }
};

void test_login(client_op const& op)
{
   boost::asio::io_context ioc;

   // Tests if the server times out connections that do not proceed
   // with the websocket handshake.
   std::make_shared< handshake_test_launcher<cmgr_handshake_tm>
                   >(ioc, op)->run({});

   // Tests if the server drops connections that connect but do not
   // register or authenticate.
   std::make_shared< handshake_test_launcher<client_mgr_accept_timer>
                   >(ioc, op)->run({});

   // Tests the sms timeout. Connections should be dropped if the
   // users tries to register but do not send the sms on time.
   // Connection is gracefully closed.
   std::make_shared<test_login_launcher>( ioc, op, 0, op.users_size, "ok"
                                        , -1)->run({});

   // Same as above but socket is shutdown.
   std::make_shared<test_login_launcher>( ioc
                                        , op, op.users_size
                                        , 2 * op.users_size
                                        , "ok"
                                        , -2)->run({});
   // Sends commands with typos.
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

   for (auto const& cmd : cmds)
      std::make_shared<client_session<client_mgr_login_typo>
                      >( ioc
                       , op.session_config()
                       , client_mgr_login_typo {cmd})->run();

   ioc.run();
}

auto test_sms( client_op const& op
             , std::string const& expected
             , std::string const& sms
             , int begin
             , int end)
{
   using mgr_type = client_mgr_sms;
   using client_type = client_session<mgr_type>;

   boost::asio::io_context ioc;

   std::vector<std::shared_ptr<client_type>> sessions;
   for (auto i = begin; i < end; ++i) {
      auto tmp =
         std::make_shared<client_type>( ioc
                                      , op.session_config()
                                      , client_mgr_sms
                                        {to_str(i, 4, 0), expected, sms});
      tmp->run();
      sessions.push_back(tmp);
   }

   ioc.run();

   std::vector<user_bind> binds;
   for (auto const& session : sessions)
      binds.push_back(session->get_mgr().bind);

   return binds;
}

auto test_auth( client_op const& op
              , std::vector<user_bind> binds)
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

      test_login(op);
      std::cout << "test_many:          ok" << std::endl;

      // Sends sms on time but the wrong one and expects the server to
      // release sessions correctly.
      test_sms(op, "fail", "8r47", 0, op.users_size);
      std::cout << "test_wrong_sms:     ok" << std::endl;

      // Sends correct sms on time.
      auto const binds = test_sms(op, "ok", "8347", 0, op.users_size);
      if (std::empty(binds)) {
         std::cerr << "Error: Binds array empty." << std::endl;
         return 1;
      }
      std::cout << "test_correct_sms:   ok" << std::endl;

      // TODO: Test this after implementing queries to the database.
      //test_login(op, "fail", 0, op.users_size, -1);
      //std::cout << "test_login_ok_4:    ok" << std::endl;

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

