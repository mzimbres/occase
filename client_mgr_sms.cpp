#include "client_mgr_sms.hpp"

#include "client_session.hpp"

client_mgr_sms::client_mgr_sms(std::string tel_)
: tel(tel_)
{ }

int client_mgr_sms::on_read(json j, std::shared_ptr<client_type> s)
{
   auto cmd = j["cmd"].get<std::string>();

   if (cmd == "login_ack") {
      auto res = j["result"].get<std::string>();
      if (res == "ok") {
         json j1;
         j1["cmd"] = "sms_confirmation";
         j1["tel"] = tel;
         j1["sms"] = "8347";
         s->send_msg(j1.dump());
         return 1;
      }

      std::cout << "Test sms_confirmation: ok (depends on server config)"
                << std::endl;
      return 1;
   }

   if (cmd == "sms_confirmation_ack") {
      //std::cout << j << std::endl;
      auto res = j["result"].get<std::string>();

      if (res == "ok") {
         bind = j["user_bind"].get<user_bind>();
         std::cout << "Test sms_confirmation: ok." << bind
                   << std::endl;
         return -1;
      }

      std::cout << "Test sms_confirmation: fail." << std::endl;
      return -1;
   }

   std::cout << "Server error: Unknown command." << std::endl;
   return -1;
}

int client_mgr_sms::on_handshake(std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "login";
   j["tel"] = tel;
   s->send_msg(j.dump());
   return 1;
}

