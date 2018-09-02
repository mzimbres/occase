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

//struct prompt_usr {
//   std::shared_ptr<client_session> p;
//   void operator()() const
//   {
//      for (;;) {
//         std::cout << "Type a command: \n\n"
//                   << "  0: Exit.\n"
//                   << "  1: Create group.\n"
//                   << "  2: Join group.\n"
//                   << "  3: Send group message.\n"
//                   << "  4: Send user message.\n"
//                   << std::endl;
//         auto cmd = -1;
//         std::cin >> cmd;
//         std::string str;
//
//         if (cmd == 0) {
//            p->prompt_close();
//            break;
//         }
//
//         if (cmd == 1) {
//            p->prompt_create_group();
//            continue;
//         }
//         
//         if (cmd == 2) {
//            p->prompt_join_group();
//            continue;
//         }
//         
//         if (cmd == 3) {
//            p->prompt_send_group_msg();
//            continue;
//         }
//
//         if (cmd == 4) {
//            p->prompt_send_user_msg();
//            continue;
//         }
//      }
//   }
//};

void test_accept_timer(client_options const& op)
{
   using mgr_type = client_mgr_accept_timer;
   using client_type = client_session<mgr_type>;

   boost::asio::io_context ioc;

   std::vector<mgr_type> mgrs {15};

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
   { {"Rabanete"}
   , {"Acafrao"}
   , {"Salsinha"}
   , {"Pimenta1"}
   , {"Pimenta2"}
   , {"Pimenta3"}
   , {"Pimenta4"}
   , {"Pimenta5"}
   , {"Pimenta6"}
   , {"Pimenta7"}
   , {"Pimenta8"}
   , {"Pimenta9"}
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

   //std::cout << "================================================"
   //          << std::endl;
   //test_accept_timer(op);
   //std::cout << "================================================"
   //          << std::endl;
   test_login(op);
   //std::cout << "================================================"
   //          << std::endl;
   //test_sms(op);
   //std::cout << "================================================"
   //          << std::endl;
   //test_client(op);

   return EXIT_SUCCESS;
}

