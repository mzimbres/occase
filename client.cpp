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

#include "config.hpp"
#include "menu_parser.hpp"
#include "client_mgr_cg.hpp"
#include "client_mgr_sms.hpp"
#include "client_mgr_sim.hpp"
#include "client_session.hpp"
#include "client_mgr_login.hpp"
#include "client_mgr_accept_timer.hpp"

void test_accept_timer(client_options const& op)
{
   using mgr_type = client_mgr_accept_timer;
   using client_type = client_session<mgr_type>;

   boost::asio::io_context ioc;

   std::vector<mgr_type> mgrs {1000};

   for (auto& mgr : mgrs)
      std::make_shared<client_type>(ioc, op, mgr)->run();

   ioc.run();
}

void test_login( client_options op
               , char const* expected
               , int begin
               , int end)
{
   using mgr_type = client_mgr_login;
   using client_type = client_session<mgr_type>;

   boost::asio::io_context ioc;

   std::vector<mgr_type> mgrs;

   for (auto i = begin; i < end; ++i)
      mgrs.push_back({to_str(i, 4, 0), expected});

   for (auto& mgr : mgrs)
      std::make_shared<client_type>(ioc, op, mgr)->run();

   ioc.run();
}

void test_flood_login( client_options op
                     , int begin
                     , int end
                     , int more)
{
   using mgr_type = client_mgr_login;
   using client_type = client_session<mgr_type>;

   boost::asio::io_context ioc;

   std::vector<mgr_type> mgrs;

   for (auto i = begin; i < end; ++i)
      mgrs.push_back({to_str(i, 4, 0), "ok"});

   for (auto i = end; i < more; ++i)
      mgrs.push_back({to_str(i, 4, 0), "fail"});

   for (auto& mgr : mgrs)
      std::make_shared<client_type>(ioc, op, mgr)->run();

   ioc.run();
}

void test_login_typo(client_options op)
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
      std::make_shared<client_type>(ioc, op, mgr)->run();

   ioc.run();
}

auto test_sms( client_options op
             , std::string const& expected
             , std::string const& sms)
{
   using mgr_type = client_mgr_sms;
   using client_type = client_session<mgr_type>;

   boost::asio::io_context ioc;

   std::vector<mgr_type> mgrs;

   for (auto i = 0; i < users_size; ++i)
      mgrs.push_back({to_str(i, 4, 0), expected, sms});

   std::vector<std::shared_ptr<client_type>> sessions;
   for (auto& mgr : mgrs)
      sessions.push_back(std::make_shared<client_type>(ioc, op, mgr));

   for (auto& session : sessions)
      session->run();

   ioc.run();

   std::vector<user_bind> binds;
   for (auto const& session : sessions)
      if (session->get_mgr().bind.index != -1)
         binds.push_back(session->get_mgr().bind);

   return binds;
}

auto test_auth(client_options op, std::vector<user_bind> binds)
{
   using mgr_type = client_mgr_auth;
   using client_type = client_session<mgr_type>;

   boost::asio::io_context ioc;

   std::vector<mgr_type> mgrs;
   for (auto& bind : binds)
      mgrs.emplace_back(bind, "ok");

   std::vector<std::shared_ptr<client_type>> sessions;
   for (auto& mgr : mgrs)
      sessions.push_back(std::make_shared<client_type>(ioc, op, mgr));

   for (auto& session : sessions)
      session->run();

   ioc.run();
}

auto test_create_group(client_options op, user_bind bind)
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
   std::make_shared<client_type>(ioc, op, mgr)->run();
   ioc.run();
}

void test_simulation(client_options op, std::vector<user_bind> binds)
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
   for (auto& mgr : mgrs)
      sessions.push_back(std::make_shared<client_type>(ioc, op, mgr));

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
      //if (argc != 2) {
      //   std::cerr << "Please, provide a user id." << std::endl;
      //   return EXIT_FAILURE;
      //}

      client_options op
      { {"127.0.0.1"} // Host.
      , {"8080"}      // Port.
      };

      std::cout << "==========================================" << std::endl;

      // Tests if the server drops connections that connect by do not
      // register or authenticate.
      test_accept_timer(op);
      std::cout << "test_accept_timer: ok" << std::endl;

      // Tests the sms timeout. Connections should be dropped if the
      // users tries to register but do not send the sms on time.
      test_login(op, "ok", 0, users_size);
      std::cout << "test_login_ok_1:   ok" << std::endl;

      // TODO: Check why the server does not release the 0 index in
      // order.

      // Tests if the previous login commands were all released. That
      // means if we send again users_size registrations there should
      // be enough indexes for them.
      test_login(op, "ok", 0, users_size);
      std::cout << "test_login_ok_2:   ok" << std::endl;

      // Sends more logins than the server has available user entries.
      // Assumes all messages will arrive in the server before the
      // first one begins to timeout.
      test_flood_login(op, 0, users_size, 50);
      std::cout << "test_flood_login:  ok" << std::endl;

      // Sends commands with typos.
      test_login_typo(op);
      std::cout << "test_login_typo:   ok" << std::endl;

      // Sends sms on time but the wrong one and expects the server to
      // release indexes correctly.
      auto binds = test_sms(op, "fail", "8r47");
      if (!std::empty(binds)) {
         std::cerr << "Error: Binds array not empty." << std::endl;
         return EXIT_FAILURE;
      }
      std::cout << "test_wrong_sms:    ok" << std::endl;

      // Sends correct sms on time.
      binds = test_sms(op, "ok", "8347");
      if (std::empty(binds)) {
         std::cerr << "Error: Binds array empty." << std::endl;
         return EXIT_FAILURE;
      }
      std::cout << "test_correct_sms:  ok" << std::endl;

      // Test if the server refuses all logins after we occupied all
      // indexes in the last sms test. First using non-existing users.
      test_login(op, "fail", users_size, 2 * users_size);
      std::cout << "test_login_ok_3:   ok" << std::endl;

      // Same as above but for already registered users.
      test_login(op, "fail", 0, users_size);
      std::cout << "test_login_ok_4:   ok" << std::endl;

      // Test authentication with binds obtained in the sms step.
      test_auth(op, binds);
      std::cout << "test_auth:         ok" << std::endl;

      test_create_group(op, binds.front());
      std::cout << "test_create_group: ok" << std::endl;

      timer t;
      test_simulation(op, binds);
      std::cout << "test_simulation:   ok" << std::endl;
      std::cout << "==========================================" << std::endl;

      return EXIT_SUCCESS;
   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
      return EXIT_FAILURE;
   }
}

