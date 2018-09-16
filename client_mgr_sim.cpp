#include "client_mgr_sim.hpp"

#include "client_session.hpp"

int client_mgr_sim::on_read(json j, std::shared_ptr<client_type> s)
{
   auto const cmd = j["cmd"].get<std::string>();

   if (cmd == "auth_ack") {
      auto const res = j["result"].get<std::string>();
      if (res == "ok") {
         //std::cout << "sending " << cmds.top() << std::endl;
         s->send_msg(cmds.top());
         return 1;
      }

      std::cout << "Test cg: Error." << std::endl;
      return -1;
   }

   if (cmd == "join_group_ack") {
      auto const res = j["result"].get<std::string>();
      if (res == expected) {
         std::cout << "Test sim: ok." << std::endl;
         cmds.pop();
         if (std::empty(cmds)) {
            // Now we begin to send some messages to the group we just
            // joined.
            json j_msg;
            j_msg["cmd"] = "send_group_msg";
            j_msg["from"] = bind;
            j_msg["to"] = "00.01.05"; // TODO: Fix this
            j_msg["msg"] = "Some random message to the group.";
            s->send_msg(j_msg.dump());
            return 1;
         }
         s->send_msg(cmds.top());
         return 1;
      }

      std::cout << "Test sim: join_group_ack fail." << std::endl;
      return -1;
   }
   if (cmd == "send_group_msg_ack") {
      auto const res = j["result"].get<std::string>();
      if (res == expected) {
         std::cout << "Test sim: ok." << std::endl;
         return 1;
      }

      std::cout << "Test sim: send_group_msg_ack: fail." << std::endl;
      return -1;
   }

   if (cmd == "group_msg") {
      auto const body = j["body"].get<std::string>();
      std::cout << "Group msg: " << body << std::endl;
      std::cout << "Test sim: ok." << std::endl;
      return -1;
   }

   std::cout << "Server error: Unknown command." << std::endl;
   return -1;
}

int client_mgr_sim::on_handshake(std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "auth";
   j["from"] = bind;
   s->send_msg(j.dump());
   return 1;
}

int client_mgr_sim::on_closed(boost::system::error_code ec)
{
   std::cout << "Test sim: fail." << std::endl;
   return -1;
};

