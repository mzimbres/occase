#include "client_mgr_cg.hpp"

#include "client_session.hpp"

int client_mgr_cg::on_read(std::string msg, std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);
   auto const cmd = j.at("cmd").get<std::string>();

   if (cmd == "auth_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res == "ok") {
         //std::cout << "Auth ok." << std::endl;
         s->send_msg(cmds.top());
         //std::cout << "Sending " << cmds.top() << std::endl;
         return 1;
      }

      std::cout << "Test auth: " << res << std::endl;
      throw std::runtime_error("client_mgr_cg::on_read1");
      return -1;
   }

   if (cmd == "create_group_ack") {
      auto const res = j["result"].get<std::string>();
      if (res == op.expected) {
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
      throw std::runtime_error("client_mgr_cg::on_read2");
      return -1;
   }

   std::cout << "Server error: Unknown command." << std::endl;
   throw std::runtime_error("client_mgr_cg::on_read3");
   return -1;
}

client_mgr_cg::client_mgr_cg(options_type op_)
: op(op_)
{
   for (auto i = 0; i < op.number_of_groups; ++i) {
      json cmd;
      cmd["cmd"] = "create_group";
      cmd["hash"] = to_str(i);
      cmds.push(cmd.dump());
   }
}

client_mgr_cg::~client_mgr_cg()
{
   std::cout << "Dtor stack: " << std::size(cmds) << std::endl;
   assert(std::empty(cmds));
}

int client_mgr_cg::on_handshake(std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "auth";
   j["from"] = op.user;
   s->send_msg(j.dump());
   return 1;
}

int client_mgr_cg::on_closed(boost::system::error_code ec)
{
   std::cout << "Test cg: fail." << std::endl;
   throw std::runtime_error("client_mgr_cg::on_read");
   return -1;
};

