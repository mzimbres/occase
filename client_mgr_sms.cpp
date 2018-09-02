#include "client_mgr_sms.hpp"

#include "client_session.hpp"

client_mgr_sms::client_mgr_sms(std::string tel_)
: tel(tel_)
{ }

int client_mgr_sms::on_read(json j, std::shared_ptr<client_type> s)
{
   auto cmd = j["cmd"].get<std::string>();

   if (cmd == "login_ack")
      return on_login_ack(std::move(j), s);

   if (cmd == "sms_confirmation_ack")
      return on_sms_confirmation_ack(std::move(j), s);

   std::cout << "Server error: Unknown command." << std::endl;
   return -1;
}

int client_mgr_sms::on_closed(boost::system::error_code ec)
{
   return 1;
}

int client_mgr_sms::on_login_ack(json j, std::shared_ptr<client_type> s)
{
   auto res = j["result"].get<std::string>();

   if (res == "ok") {
      //std::cout << "login_ack: ok." << std::endl;
      send_ok_sms_confirmation(s);
      return 1;
   }

   std::cout << "client_mgr_sms::on_login_ack: SERVER ERROR. Please report."
             << std::endl;
   return 1;
}

int
client_mgr_sms::on_sms_confirmation_ack( json j
                                       , std::shared_ptr<client_type> s)
{
   //std::cout << j << std::endl;
   auto res = j["result"].get<std::string>();

   if (res == "ok") {
      bind = j["user_bind"].get<user_bind>();
      std::cout << "Test sms_confirmation: ok." << std::endl;
      return -1;
   }

   std::cout << "Test sms_confirmation: fail." << std::endl;
   return -1;
}

void client_mgr_sms::send_ok_login(std::shared_ptr<client_type> s)
{
   // What if we try more than one ok login?
   json j;
   j["cmd"] = "login";
   j["tel"] = tel;
   s->send_msg(j.dump());
}

void client_mgr_sms::send_ok_sms_confirmation(std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "sms_confirmation";
   j["tel"] = tel;
   j["sms"] = "8347";
   s->send_msg(j.dump());
}

int client_mgr_sms::on_handshake(std::shared_ptr<client_type> s)
{
   if (number_of_ok_logins-- > 0) {
      send_ok_login(s);
      return 1;
   }

   std::cout << "Test sms_confirmation: failed." << std::endl;
   return -1;
}

