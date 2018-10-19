#include "client_mgr_sim.hpp"

#include "menu_parser.hpp"
#include "client_session.hpp"

client_mgr_sim::client_mgr_sim(options_type op_)
: op(op_)
{
   for (auto i = 0; i < op.number_of_groups; ++i) {
      auto const hash = to_str(i);
      hashes.push(hash);
      json cmd;
      cmd["cmd"] = "join_group";
      cmd["hash"] = hash;
      cmds.push(cmd.dump());
   }
}

int client_mgr_sim::on_read(std::string msg, std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);
   auto const cmd = j.at("cmd").get<std::string>();

   if (cmd == "auth_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res == "ok") {
         //std::cout << "sending " << cmds.top() << std::endl;
         s->send_msg(cmds.top());
         return 1;
      }

      std::cout << "Test sim: Error." << std::endl;
      throw std::runtime_error("client_mgr_sim::on_read1");
      return -1;
   }

   if (cmd == "join_group_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res == op.expected) {
         if (std::empty(cmds))
            throw std::runtime_error("Stack not suposed to be empty.");

         cmds.pop();
         if (std::empty(cmds)) {
            //std::cout << "Test sim: join groups ok." << std::endl;
            send_group_msg(s);
            return 1;
         }
         //std::cout << "sending " << cmds.top() << std::endl;
         s->send_msg(cmds.top());
         return 1;
      }

      std::cout << "Test sim: join_group_ack fail." << std::endl;
      throw std::runtime_error("client_mgr_sim::on_read2");
      return -1;
   }
   if (cmd == "group_msg_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res == op.expected) {
         if (++counter == op.msgs_per_group) {
            hashes.pop();
            counter = 0;
         }

         if (std::empty(hashes)) {
            //std::cout << "Test sim: send_group_msg_ack ok." << std::endl;
            return -1;
         }
         send_group_msg(s);
         return 1;
      }

      std::cout << "Test sim: send_group_msg_ack: fail." << std::endl;
      throw std::runtime_error("client_mgr_sim::on_read3");
      return -1;
   }

   if (cmd == "group_msg") {
      // TODO: Output some error if the number of messages received is
      // wrong.
      //auto const body = j.at("msg").get<std::string>();
      //std::cout << "Group msg: " << body << std::endl;
      return 1;
   }

   std::cout << "Server error: Unknown command." << std::endl;
   throw std::runtime_error("client_mgr_sim::on_read4");
   return -1;
}

int client_mgr_sim::on_handshake(std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "auth";
   j["from"] = op.user;
   s->send_msg(j.dump());
   //std::cout << "Sending " << j.dump() << std::endl;
   return 1;
}

int client_mgr_sim::on_closed(boost::system::error_code ec)
{
   std::cout << "Test sim: fail." << std::endl;
   throw std::runtime_error("client_mgr_sim::on_closed");
   return -1;
};

void client_mgr_sim::send_group_msg(std::shared_ptr<client_type> s)
{
   json j_msg;
   j_msg["cmd"] = "group_msg";
   j_msg["to"] = hashes.top();
   j_msg["msg"] = "Group message to: " + hashes.top();
   s->send_msg(j_msg.dump());
}

