#include "client_mgr_pub.hpp"

#include "menu_parser.hpp"
#include "client_session.hpp"

namespace rt
{

client_mgr_pub::client_mgr_pub(options_type op_)
: op(op_)
{
}

int client_mgr_pub::on_read(std::string msg, std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);
   //std::cout << j << std::endl;
   auto const cmd = j.at("cmd").get<std::string>();

   if (cmd == "auth_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res == "ok") {
         auto const menu_str = j.at("menu").get<std::string>();
         auto const jmenu = json::parse(menu_str);
         auto const channels = get_hashes(jmenu);
         for (auto const& o : channels)
            for (auto i = 0; i < op.msgs_per_group; ++i)
               hashes.push_back({false, false, o});
         json j_sub;
         j_sub["cmd"] = "subscribe";
         j_sub["channels"] = channels;
         s->send_msg(j_sub.dump());
         return 1;
      }

      std::cout << "Test sim: Error." << std::endl;
      throw std::runtime_error("client_mgr_pub::on_read1");
      return -1;
   }

   if (cmd == "subscribe_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res == op.expected) {
         auto const count = j.at("count").get<int>();
         std::cout << "subscribe ok: " << count << std::endl;
         send_group_msg(s, 0);
         return 1;
      }

      std::cout << "Test sim: subscribe_ack fail." << std::endl;
      throw std::runtime_error("client_mgr_pub::on_read2");
      return -1;
   }

   if (cmd == "publish_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res == op.expected) {
         auto const id = j.at("id").get<int>();
         //std::cout << "Receiving publish_ack: " << op.user << " " << id << " " << hashes.at(id).hash << std::endl;
         if (hashes.at(id).ack)
            throw std::runtime_error("client_mgr_pub::on_read4");
         hashes.at(id).ack = true;
         return 1;
      }

      std::cout << "Test sim: send_group_msg_ack: fail." << std::endl;
      throw std::runtime_error("client_mgr_pub::on_read3");
      return -1;
   }

   if (cmd == "publish") {
      //auto const body = j.at("msg").get<std::string>();
      //std::cout << "Group msg: " << body << std::endl;
      //std::cout << j << std::endl;
      auto const from = j.at("from").get<std::string>();
      //std::cout << from << " != " << op.user << std::endl;
      if (from != op.user)
         return 1;

      auto const id = j.at("id").get<unsigned>();
      if (hashes.at(id).msg)
         throw std::runtime_error("client_mgr_pub::on_read5");

      hashes.at(id).msg = true;

      //std::cout << id << " " << std::size(hashes) << std::endl;
      if (id == std::size(hashes) - 1) {
         //std::cout << id << " " << std::size(hashes) << std::endl;
         for (auto const& o : hashes)
            if (!o.ack || !o.msg)
               std::cout << "client_mgr_pub: Test fails." << std::endl;

         std::cout << "FINISH group messages." << op.user  << std::endl;
         return -1;
      }

      //std::cout << "Receiving publish:     " << op.user << " " << id
      //          << " " << hashes.at(id).hash <<std::endl;
      send_group_msg(s, id + 1);
      return 1;
   }

   std::cout << "Server error: Unknown command." << std::endl;
   throw std::runtime_error("client_mgr_pub::on_read4");
   return -1;
}

int client_mgr_pub::on_handshake(std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "auth";
   j["from"] = op.user;
   j["menu_version"] = -1;
   s->send_msg(j.dump());
   //std::cout << "Sending " << j.dump() << std::endl;
   return 1;
}

int client_mgr_pub::on_closed(boost::system::error_code ec)
{
   std::cout << "Test sim: fail." << std::endl;
   throw std::runtime_error("client_mgr_pub::on_closed");
   return -1;
};

void client_mgr_pub::send_group_msg( std::shared_ptr<client_type> s
                                   , int c) const
{
   // For each one of these messages sent we shall receive first one
   // server ack and then it again from the broadcast channel it was
   // sent.
   json j_msg;
   j_msg["cmd"] = "publish";
   j_msg["from"] = op.user;
   j_msg["to"] = hashes.at(c).hash;
   j_msg["msg"] = "Group message";
   j_msg["id"] = c;
   s->send_msg(j_msg.dump());
   //std::cout << "Sending   publish      " << op.user << " "
   //          << c << " " << hashes.at(c).hash
   //          << std::endl;
}

client_mgr_pub::~client_mgr_pub()
{
}

}

