#include <stack>
#include <thread>
#include <memory>
#include <vector>
#include <string>
#include <sstream>
#include <cstdlib>
#include <iostream>

#include "config.hpp"
#include "client_mgr.hpp"
#include "client_mgr_cg.hpp"
#include "client_mgr_sms.hpp"
#include "client_session.hpp"
#include "client_mgr_login.hpp"
#include "client_mgr_accept_timer.hpp"

void test_accept_timer(client_options const& op)
{
   using mgr_type = client_mgr_accept_timer;
   using client_type = client_session<mgr_type>;

   boost::asio::io_context ioc;

   std::vector<mgr_type> mgrs {10};

   for (auto& mgr : mgrs)
      std::make_shared<client_type>(ioc, op, mgr)->run();

   ioc.run();
}

void test_login(client_options op)
{
   using mgr_type = client_mgr_login;
   using client_type = client_session<mgr_type>;

   boost::asio::io_context ioc;

   // Assumes all the commands arrive at the server before the first
   // ones begin to timeout. The fail means no more user entries
   // available.
   std::vector<mgr_type> mgrs
   { {"aaa", "ok"}
   , {"bbb", "ok"}
   , {"ccc", "ok"}
   , {"ddd", "ok"}
   , {"eee", "ok"}
   , {"ddd", "fail"}
   , {"fff", "ok"}
   , {"ggg", "ok"}
   , {"hhh", "ok"}
   , {"iii", "ok"}
   , {"kkk", "ok"}
   , {"lll", "fail"}
   , {"mmm", "fail"}
   , {"nnn", "fail"}
   , {"ooo", "fail"}
   , {"ppp", "fail"}
   };

   for (auto& mgr : mgrs)
      std::make_shared<client_type>(ioc, op, mgr)->run();

   ioc.run();
}

void test_login1(client_options op)
{
   using mgr_type = client_mgr_login1;
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

auto test_sms(client_options op)
{
   using mgr_type = client_mgr_sms;
   using client_type = client_session<mgr_type>;

   boost::asio::io_context ioc;

   std::vector<mgr_type> mgrs
   { {"Melao",   "ok",   "8347"}
   , {"Fruta",   "fail", "8337"}
   , {"Poka",    "ok",   "8347"}
   , {"Abobora", "ok",   "8347"}
   , {"ddda",    "fail", "8947"}
   , {"hjsjs",   "ok",   "8347"}
   , {"9899",    "ok",   "8347"}
   , {"87z",     "ok",   "8347"}
   , {"7162",    "fail", "1347"}
   , {"2763333", "ok",   "8347"}
   };

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

auto gen_menu_json()
{
   std::vector<json> j1 =
   { {{"name", "Centro"},               {"sub", {}}}
   , {{"name", "Alvinópolis"},          {"sub", {}}}
   , {{"name", "Jardim Siriema"},       {"sub", {}}}
   , {{"name", "Vila Santista"},        {"sub", {}}}
   , {{"name", "Parque dos Coqueiros"}, {"sub", {}}}
   , {{"name", "Terceiro Centenário"},  {"sub", {}}}
   };

   std::vector<json> j2 =
   { {{"name", "Vila Leopoldina"}, {"sub", {}}}
   , {{"name", "Lapa"},            {"sub", {}}}
   , {{"name", "Pinheiros"},       {"sub", {}}}
   , {{"name", "Moema"},           {"sub", {}}}
   , {{"name", "Jardim Paulista"}, {"sub", {}}}
   , {{"name", "Mooca"},           {"sub", {}}}
   , {{"name", "Tatuapé"},         {"sub", {}}}
   , {{"name", "Penha"},           {"sub", {}}}
   , {{"name", "Ipiranga"},        {"sub", {}}}
   , {{"name", "Vila Madalena"},   {"sub", {}}}
   , {{"name", "Vila Mariana"},    {"sub", {}}}
   , {{"name", "Vila Formosa"},    {"sub", {}}}
   , {{"name", "Bixiga"},          {"sub", {}}}
   };

   std::vector<json> j3 =
   { {{"name", "Atibaia"},  {"sub", j1}}
   , {{"name", "Campinas"}, {"sub", {}}}
   , {{"name", "Sao Paulo"},{"sub", j2}}
   , {{"name", "Piracaia"}, {"sub", {}}}
   };

   json j;
   j["name"] = "SP";
   j["sub"] = j3;

   return j.dump();
}

void parse_menu_json(std::string menu)
{
   //std::cout << menu << std::endl;

   if (std::empty(menu))
      return;

   json j;
   std::stringstream ss;
   ss << menu;
   ss >> j;

   std::stack<std::vector<json>> st;
   st.push({j});
   do {
      while (!st.top().back()["sub"].is_null()) {
         auto const vec = st.top().back()["sub"].get<std::vector<json>>();
         st.push(std::move(vec));
      }

      auto const name1 = st.top().back()["name"].get<std::string>();
      std::cout << name1 << std::endl;
      st.top().pop_back();

      if (!std::empty(st.top()))
         continue;
      
      st.pop();

      auto const name2 = st.top().back()["name"].get<std::string>();
      std::cout << name2 << std::endl;

      st.top().pop_back();

      if (std::empty(st.top()))
         st.pop();

   } while (std::size(st) != 1);
}

auto test_cg(client_options op, user_bind bind)
{
   using mgr_type = client_mgr_cg;
   using client_type = client_session<mgr_type>;

   boost::asio::io_context ioc;

   std::stack<std::string> st;

   json cg1;
   cg1["cmd"] = "create_group";
   cg1["from"] = bind;
   cg1["hash"] = "000.000.000.000";
   cg1["info"] = group_info {"Atibaia", "Centro"};
   st.push(cg1.dump());

   json cg2;
   cg2["cmd"] = "create_group";
   cg2["from"] = bind;
   cg2["hash"] = "000.000.000.001";
   cg2["info"] = group_info {"Atibaia", "Alvinopolis"};
   st.push(cg2.dump());

   mgr_type mgr {"ok", std::move(st), bind};

   std::make_shared<client_type>(ioc, op, mgr)->run();

   ioc.run();
}

void test_client(client_options op)
{
   using mgr_type = client_mgr;
   using client_type = client_session<mgr_type>;

   boost::asio::io_context ioc;

   mgr_type mgr("Mandioca");
   auto p = std::make_shared<client_type>(ioc, std::move(op), mgr);

   p->run();
   ioc.run();
}

int main(int argc, char* argv[])
{
   //if (argc != 2) {
   //   std::cerr << "Please, provide a user id." << std::endl;
   //   return EXIT_FAILURE;
   //}

   parse_menu_json(gen_menu_json());
   return 0;

   client_options op
   { {"127.0.0.1"} // Host.
   , {"8080"}      // Port.
   };

   std::cout << "==========================================" << std::endl;
   test_accept_timer(op);
   std::cout << "==========================================" << std::endl;
   test_login(op);
   std::cout << "==========================================" << std::endl;
   test_login1(op);
   std::cout << "==========================================" << std::endl;
   auto binds = test_sms(op);
   if (std::empty(binds)) {
      std::cerr << "Binds array empty." << std::endl;
      return EXIT_FAILURE;
   }
   std::cout << "==========================================" << std::endl;
   test_auth(op, binds);
   std::cout << "==========================================" << std::endl;
   test_cg(op, binds.front());
   std::cout << "==========================================" << std::endl;
   //test_client(op);
   //std::cout << "==========================================" << std::endl;

   return EXIT_SUCCESS;
}

