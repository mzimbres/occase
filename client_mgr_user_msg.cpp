#include "client_mgr_user_msg.hpp"

#include "menu_parser.hpp"
#include "client_session.hpp"

namespace rt
{

client_mgr_user_msg::client_mgr_user_msg(options_type op_)
: op(op_)
{
}

int
client_mgr_user_msg::on_read( std::string msg
                            , std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);
   //std::cout << j << std::endl;
   auto const cmd = j.at("cmd").get<std::string>();

   if (cmd == "auth_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res == "ok") {
         return 1;
      }

      std::cout << "Test sim: Error." << std::endl;
      throw std::runtime_error("client_mgr_user_msg::on_read1");
      return -1;
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
   //std::cout << "Sending   user_msg       " << op.user
   //          << " " << user_counter << std::endl;
   user_counter++;
}

client_mgr_user_msg::~client_mgr_user_msg()
{
}

}

