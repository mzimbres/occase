#include <thread>
#include <memory>
#include <vector>
#include <string>
#include <cstdlib>
#include <iostream>

#include "config.hpp"
#include "client_mgr.hpp"
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

   std::vector<mgr_type> mgrs
   { {"aaa"}
   , {"bbb"}
   , {"ccc"}
   , {"ddd"}
   , {"eee"}
   , {"ddd"}
   , {"fff"}
   , {"ggg"}
   , {"hhh"}
   , {"iii"}
   , {"kkk"}
   , {"lll"}
   , {"mmm"}
   , {"nnn"}
   , {"ooo"}
   , {"ppp"}
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
void test_sms(client_options op)
{
   using mgr_type = client_mgr_sms;
   using client_type = client_session<mgr_type>;

   boost::asio::io_context ioc;

   mgr_type mgr("Melao");
   auto p = std::make_shared<client_type>(ioc, std::move(op), mgr);

   p->run();
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

   client_options op
   { {"127.0.0.1"} // Host.
   , {"8080"}      // Port.
   };

   std::cout << "================================================"
             << std::endl;
   test_accept_timer(op);
   std::cout << "================================================"
             << std::endl;
   test_login(op);
   std::cout << "================================================"
             << std::endl;
   test_login1(op);
   std::cout << "================================================"
             << std::endl;
   test_sms(op);
   std::cout << "================================================"
             << std::endl;
   test_client(op);

   return EXIT_SUCCESS;
}

