#include "client_mgr_sim.hpp"

#include "menu_parser.hpp"
#include "client_session.hpp"

client_mgr_sim::client_mgr_sim(options_type op_)
: op(op_)
{
   for (auto i = 0; i < op.number_of_groups; ++i) {
      auto const hash = to_str(i);
      for (auto i = 0; i < op.msgs_per_group; ++i)
         hashes.push_back({false, false, hash});
      json cmd;
      cmd["cmd"] = "join_group";
      cmd["hash"] = hash;
      cmds.push(cmd.dump());
   }
}

int client_mgr_sim::on_read(std::string msg, std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);
   //std::cout << j << std::endl;
   auto const cmd = j.at("cmd").get<std::string>();

   if (cmd == "auth_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res == "ok") {
         //std::cout << "Sending " << cmds.top() << std::endl;
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
         //std::cout << "join_group_ack: " << op.user << std::endl;
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
         auto const id = j.at("id").get<int>();
         if (hashes.at(id).ack)
            throw std::runtime_error("client_mgr_sim::on_read4");
         hashes.at(id).ack = true;
         //std::cout << "Receiving group_msg_ack: " << op.user << " " << id << " " << hashes.at(id).hash << std::endl;
         return 1;
      }

      std::cout << "Test sim: send_group_msg_ack: fail." << std::endl;
      throw std::runtime_error("client_mgr_sim::on_read3");
      return -1;
   }

   if (cmd == "group_msg") {
      //auto const body = j.at("msg").get<std::string>();
      //std::cout << "Group msg: " << body << std::endl;
      //std::cout << j << std::endl;
      auto const from = j.at("from").get<std::string>();
      if (from != op.user) {
         users_tmp.insert(from);
         //std::cout << "Pushing on " << op.user << " stack: " << from
         //          << std::endl;
         return 1;
      }

      auto const id = j.at("id").get<int>();
      if (hashes.at(id).msg)
         throw std::runtime_error("client_mgr_sim::on_read5");

      hashes.at(id).msg = true;

      if (group_counter == std::size(hashes)) {
         for (auto const& o : hashes)
            if (!o.ack || !o.msg)
               std::cout << "client_mgr_sim: Test fails." << std::endl;

         //std::cout << "FINISH group messages." << std::endl;
         // Now we begin sending messages to the users from which we
         // received any group message.
         for (auto const& o : users_tmp)
            users.push(o);

         if (std::empty(users)) {
            //std::cout << "Users stack empty. Leaving ..." << std::endl;
            return -1;
         }

         //std::cout << "Users stack size " << std::size(users) << std::endl;
         send_user_msg(s);
         return 1;
      }

      //std::cout << "Receiving group_msg:     " << op.user << " " << id << " " << hashes.at(id).hash <<std::endl;
      send_group_msg(s);
      return 1;
   }

   if (cmd == "user_msg_server_ack") {
      //auto const id = j.at("id").get<int>();
      //std::cout << "Ack received from " << id << std::endl;
      users.pop();
      if (std::empty(users))
         return -1;
      send_user_msg(s);
      return 1;
   }

   if (cmd == "user_msg") {
      auto const msg = j.at("msg").get<std::string>();
      std::cout << msg << std::endl;
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
   // For each one of these messages sent we shall receive first one
   // server ack and then it again from the broadcast channel it was
   // sent.
   json j_msg;
   j_msg["cmd"] = "group_msg";
   j_msg["from"] = op.user;
   j_msg["to"] = hashes.at(group_counter).hash;
   j_msg["msg"] = "Group message";
   j_msg["id"] = group_counter;
   s->send_msg(j_msg.dump());
   //std::cout << "Sending   group_msg      " << op.user << " " << group_counter << " " << hashes.at(group_counter).hash << std::endl;
   group_counter++;
}

void client_mgr_sim::send_user_msg(std::shared_ptr<client_type> s)
{
   json j_msg;
   j_msg["cmd"] = "user_msg";
   j_msg["from"] = op.user;
   j_msg["to"] = users.top(); // TODO
   j_msg["msg"] = "User message";
   j_msg["id"] = user_counter;
   s->send_msg(j_msg.dump());
   //std::cout << "Sending   user_msg       " << op.user << " " << user_counter << std::endl;
   user_counter++;
}

client_mgr_sim::~client_mgr_sim()
{
}

