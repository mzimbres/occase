#include "client_mgr_sms.hpp"

#include "client_session.hpp"

namespace rt
{

int client_mgr_code::on_read(std::string msg, std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);
   auto const cmd = j["cmd"].get<std::string>();

   if (cmd == "register_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res == "ok") {
         json j1;
         j1["cmd"] = "code_confirmation";
         j1["tel"] = op.user;
         j1["code"] = op.code;
         s->send_msg(j1.dump());
         //std::cout << "register_ack ok: " << op.user << std::endl;
         return 1;
      }

      throw std::runtime_error("client_mgr_code::on_read1");
      return -1;
   }

   if (cmd == "code_confirmation_ack") {
      //std::cout << j << std::endl;
      auto const res = j.at("result").get<std::string>();

      if (res == op.expected) {
         // Successfull end
         //std::cout << "Test code_confirmation: ok." << std::endl;
         return op.end_ret;
      }

      throw std::runtime_error("client_mgr_code::on_read2");
      return -1;
   }

   throw std::runtime_error("client_mgr_code::on_read3");
   return -1;
}

int client_mgr_code::on_handshake(std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "register";
   j["tel"] = op.user;
   s->send_msg(j.dump());
   return 1;
}

//____________________________________________________________

int client_mgr_auth::on_read(std::string msg, std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);
   auto const cmd = j["cmd"].get<std::string>();

   if (cmd == "auth_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res == op.expected) {
         //std::cout << "Test auth: ok." << std::endl;
         return -1;
      }

      throw std::runtime_error("client_mgr_auth::on_read");
      return -1;
   }

   throw std::runtime_error("client_mgr_auth::on_read");
   return -1;
}

int client_mgr_auth::on_handshake(std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "auth";
   j["from"] = op.user;
   s->send_msg(j.dump());
   return 1;
}

}

