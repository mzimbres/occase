#include "client_mgr_cg.hpp"

#include "client_session.hpp"

int client_mgr_cg::on_read(json j, std::shared_ptr<client_type> s)
{
   auto cmd = j["cmd"].get<std::string>();

   if (cmd == "auth_ack") {
      auto res = j["result"].get<std::string>();
      if (res == expected) {
         std::cout << "Test cg: ok." << std::endl;
         return -1;
      }

      std::cout << "Test cg: fail." << std::endl;
      return -1;
   }

   std::cout << "Server error: Unknown command." << std::endl;
   return -1;
}

int client_mgr_cg::on_handshake(std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "auth";
   j["from"] = bind;
   s->send_msg(j.dump());
   return 1;
}

int client_mgr_cg::on_closed(boost::system::error_code ec)
{
   std::cout << "Test cg: fail." << std::endl;
   return -1;
};

