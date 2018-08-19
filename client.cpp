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
                   << "  0: Exit.\n"
                   << "  1: Create group.\n"
                   << "  2: Join group.\n"
                   << "  3: Send group message.\n"
                   << "  4: Send user message.\n"
                   << std::endl;
         auto cmd = -1;
         std::cin >> cmd;
         std::string str;

         if (cmd == 0) {
            p->prompt_close();
            break;
         }

         if (cmd == 1) {
            p->prompt_create_group();
            continue;
         }
         
         if (cmd == 2) {
            p->prompt_join_group();
            continue;
         }
         
         if (cmd == 3) {
            p->prompt_send_group_msg("Mensagem ao grupo.");
            continue;
         }

         if (cmd == 4) {
            p->prompt_send_user_msg("Mensagem particular a um membro.");
            continue;
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

   client_options op
   { {"127.0.0.1"}                  // Host.
   , {"8080"}                       // Port.
   , argv[1]                        // User telefone.
   , false                          // Sets interative mode.
   , 103                            // Number of groups to create.
   , std::chrono::milliseconds{500} // Interval for groups creation.
   , 103                            // Number of joins.
   , std::chrono::milliseconds{500} // Interval between joins.
   };

   boost::asio::io_context ioc;

   auto p = std::make_shared<client_session>(ioc, std::move(op));

   std::thread thr {prompt_usr {p}};

   p->run();
   ioc.run();
   thr.join();

   return EXIT_SUCCESS;
}

