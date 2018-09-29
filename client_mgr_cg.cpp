#include "client_mgr_cg.hpp"

#include "client_session.hpp"

int client_mgr_cg::on_read(json j, std::shared_ptr<client_type> s)
{
   auto const cmd = j["cmd"].get<std::string>();

   if (cmd == "create_group_ack") {
      auto const res = j["result"].get<std::string>();
      if (res == expected) {
         //std::cout << "Test cg: ok." << std::endl;
         //std::cout << "poping " << cmds.top() << std::endl;
         cmds.pop();
         if (std::empty(cmds)) {
            //std::cout << "Stack empty." << std::endl;
            return -1;
         }
         //std::cout << "sending " << cmds.top() << std::endl;
         s->send_msg(cmds.top());
         return 1;
      }

      std::cout << "Test cg: create_group_ack fail." << std::endl;
      throw std::runtime_error("client_mgr_cg::on_read");
      return -1;
   }

   std::cout << "Server error: Unknown command." << std::endl;
   throw std::runtime_error("client_mgr_cg::on_read");
   return -1;
}

client_mgr_cg::~client_mgr_cg()
{
   // TODO: Why is this constructor being called twice?
   //std::cout << "Stack: " << std::empty(cmds) << std::endl;
   //assert(std::empty(cmds));
}

int client_mgr_cg::on_handshake(std::shared_ptr<client_type> s)
{
   s->send_msg(cmds.top());
   return 1;
}

int client_mgr_cg::on_closed(boost::system::error_code ec)
{
   std::cout << "Test cg: fail." << std::endl;
   throw std::runtime_error("client_mgr_cg::on_read");
   return -1;
};

