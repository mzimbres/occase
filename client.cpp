#include <thread>
#include <memory>
#include <string>
#include <cstdlib>
#include <iostream>

#include "config.hpp"
#include "client_session.hpp"

struct prompt_usr {
   std::shared_ptr<client_session> p;
   void operator()() const
   {
      for (;;) {
         std::cout << "Type a command: \n\n"
                   << "  1: Create group.\n"
                   << "  2: Join group.\n"
                   << "  3: Send group message.\n"
                   << "  4: Exit.\n"
                   << std::endl;
         auto cmd = -1;
         std::cin >> cmd;
         std::string str;

         if (cmd == 1) {
            p->create_group();
            continue;
         }
         
         if (cmd == 2) {
            p->join_group();
            continue;
         }
         
         if (cmd == 3) {
            p->send_group_msg("Fala mulecada.");
            continue;
         }
         
         if (cmd == -1) {
            p->exit();
            break;
         }
      }
   }
};

int main(int argc, char* argv[])
{
   if (argc != 2) {
      std::cerr << "Please, provide a user id." << std::endl;
      return EXIT_FAILURE;
   }

   boost::asio::io_context ioc;

   auto p = std::make_shared<client_session>(ioc, argv[1]);

   std::thread thr {prompt_usr {p}};

   p->run();
   ioc.run();
   thr.join();

   return EXIT_SUCCESS;
}

