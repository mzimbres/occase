#include "client_mgr_login.hpp"

#include "client_session.hpp"

client_mgr_login::client_mgr_login(std::string tel_)
: tel(tel_)
{ }

int client_mgr_login::on_read(json j, std::shared_ptr<client_type> s)
{
   auto cmd = j["cmd"].get<std::string>();

   if (cmd != "login_ack") {
      std::cerr << "Server error. Please fix." << std::endl;
      return -1;
   }

   auto res = j["result"].get<std::string>();
   if (res == "ok") {
      std::cout << "Test login: ok." << std::endl;
      return -1;
   }

   if (res == "fail") {
      std::cout << "Test login: ok (if users in server exausted)."
                << std::endl;
      return -1;
   }

   std::cerr << "Server error. Please fix." << std::endl;
   return -1;
}

int client_mgr_login::on_closed(boost::system::error_code ec)
{
   return -1;
}

int client_mgr_login::on_handshake(std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "login";
   j["tel"] = tel;
   s->send_msg(j.dump());
   return 1;
}

//___________________________________________________________________

client_mgr_login1::client_mgr_login1(std::string cmd_)
: cmd(cmd_)
{ }

int client_mgr_login1::on_read(json j, std::shared_ptr<client_type> s)
{
   // A dropped login should not receive any message.
   std::cout << "Test login1: fail." << std::endl;
   return -1;
}

int client_mgr_login1::on_closed(boost::system::error_code ec)
{
   std::cout << "Test login1: ok." << std::endl;
   return -1;
}

int client_mgr_login1::on_handshake(std::shared_ptr<client_type> s)
{
   s->send_msg(cmd);
   return 1;
}

