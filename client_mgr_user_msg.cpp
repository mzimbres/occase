#include "client_mgr_user_msg.hpp"

#include "menu_parser.hpp"
#include "client_session.hpp"

namespace rt
{

client_mgr_user_msg::client_mgr_user_msg(options_type op_)
: op(op_)
{
}

int client_mgr_user_msg::on_read(std::string msg, std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);
   //std::cout << j << std::endl;
   auto const cmd = j.at("cmd").get<std::string>();

   if (cmd == "auth_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res == "ok") {
         //std::cout << "Sending " << cmds.top() << std::endl;
         auto const menu_str = j.at("menu").get<std::string>();
         auto const jmenu = json::parse(menu_str);
         auto const h = get_hashes(jmenu);
         for (auto const& o : h) {
            for (auto i = 0; i < op.msgs_per_group; ++i)
               hashes.push_back({false, false, o});
            json cmd;
            cmd["cmd"] = "subscribe_ack";
            cmd["hash"] = o;
            cmds.push(cmd.dump());
         }
         s->send_msg(cmds.top());
         return 1;
      }

      std::cout << "Test sim: Error." << std::endl;
      throw std::runtime_error("client_mgr_user_msg::on_read1");
      return -1;
   }

   if (cmd == "subscribe_ack") {
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
         //std::cout << "subscribe_ack: " << op.user << std::endl;
         s->send_msg(cmds.top());
         return 1;
      }

      std::cout << "Test sim: subscribe_ack fail." << std::endl;
      throw std::runtime_error("client_mgr_user_msg::on_read2");
      return -1;
   }

   if (cmd == "group_msg_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res == op.expected) {
         auto const id = j.at("id").get<int>();
         if (hashes.at(id).ack)
            throw std::runtime_error("client_mgr_user_msg::on_read4");
         hashes.at(id).ack = true;
         //std::cout << "Receiving group_msg_ack: " << op.user << " " << id << " " << hashes.at(id).hash << std::endl;
         return 1;
      }

      std::cout << "Test sim: send_group_msg_ack: fail." << std::endl;
      throw std::runtime_error("client_mgr_user_msg::on_read3");
      return -1;
   }

   if (cmd == "group_msg") {
      //auto const body = j.at("msg").get<std::string>();
      //std::cout << "Group msg: " << body << std::endl;
      //std::cout << j << std::endl;
      auto const from = j.at("from").get<std::string>();
      //std::cout << from << " != " << op.user << std::endl;
      if (from != op.user) {
         users.insert(from);
         //std::cout << "Pushing on " << op.user << " stack: " << from
         //          << std::endl;
         return 1;
      }

      auto const id = j.at("id").get<int>();
      if (hashes.at(id).msg)
         throw std::runtime_error("client_mgr_user_msg::on_read5");

      hashes.at(id).msg = true;

      //std::cout << group_counter << " " << std::size(hashes) << std::endl;
      if (group_counter == std::size(hashes)) {
         //std::cout << group_counter << " " << std::size(hashes) << std::endl;
         for (auto const& o : hashes)
            if (!o.ack || !o.msg)
               std::cout << "client_mgr_user_msg: Test fails." << std::endl;

         //std::cout << "FINISH group messages." << std::endl;
         // Now we begin sending messages to the users from which we
         // received any group message.
         if (std::empty(users)) {
            std::cout << "Users set empty. Leaving ..." << std::endl;
            return -1;
         }

         //for (auto const& o : users)
         //   send_user_msg(s, o);

         return -1;
      }

      //std::cout << "Receiving group_msg:     " << op.user << " " << id << " " << hashes.at(id).hash <<std::endl;
      send_group_msg(s);
      return 1;
   }

   if (cmd == "user_msg_server_ack") {
      //auto const id = j.at("id").get<int>();
      //std::cout << "Server ack received from " << id << std::endl;
      return 1;
   }

   if (cmd == "user_msg") {
      auto const from = j.at("from").get<std::string>();
      auto const msg = j.at("msg").get<std::string>();
      std::cout << from << " " << msg << std::endl;
      return 1;
   }

   std::cout << "Server error: Unknown command." << std::endl;
   throw std::runtime_error("client_mgr_user_msg::on_read4");
   return -1;
}

int client_mgr_user_msg::on_handshake(std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "auth";
   j["from"] = op.user;
   j["menu_version"] = -1;
   s->send_msg(j.dump());
   //std::cout << "Sending " << j.dump() << std::endl;
   return 1;
}

int client_mgr_user_msg::on_closed(boost::system::error_code ec)
{
   std::cout << "Test sim: fail." << std::endl;
   throw std::runtime_error("client_mgr_user_msg::on_closed");
   return -1;
};

void client_mgr_user_msg::send_group_msg(std::shared_ptr<client_type> s)
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
   //std::cout << "Sending   group_msg      " << op.user << " "
   //          << group_counter << " " << hashes.at(group_counter).hash
   //          << std::endl;
   group_counter++;
}

void client_mgr_user_msg::send_user_msg( std::shared_ptr<client_type> s
                                  , std::string to)
{
   json j_msg;
   j_msg["cmd"] = "user_msg";
   j_msg["from"] = op.user;
   j_msg["to"] = to;
   j_msg["msg"] = "User message";
   j_msg["id"] = user_counter;
   s->send_msg(j_msg.dump());
   //std::cout << "Sending   user_msg       " << op.user << " " << user_counter << std::endl;
   user_counter++;
}

client_mgr_user_msg::~client_mgr_user_msg()
{
}

}

