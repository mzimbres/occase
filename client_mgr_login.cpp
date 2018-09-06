#include "client_mgr_login.hpp"

#include "client_session.hpp"

client_mgr_login::client_mgr_login(std::string tel_)
: tel(tel_)
{ }

int client_mgr_login::on_read(json j, std::shared_ptr<client_type> s)
{
   auto cmd = j["cmd"].get<std::string>();

   if (cmd != "login_ack") {
      // Since we are only sending login commands only a login_ack is
      // expected, anything else is an error.
      std::cerr << "Server error. Please fix." << std::endl;
      return -1;
   }

   auto res = j["result"].get<std::string>();
   if (res != "ok") {
      // We still have no fail login server ack.
      std::cout << "SERVER ERROR: client_mgr_login::on_read.\n"
                << "Please fix." << std::endl;
      return 1;
   }

   //std::cout << "login_ack: ok." << std::endl;
   if (--number_of_ok_logins == 0) {
      std::cout << "Test login: ok." << std::endl;
      return -1;
   }

   std::cout << "Test login: fail." << std::endl;
   return -1;
}

int client_mgr_login::on_closed(boost::system::error_code ec)
{
   return 1;
}

void client_mgr_login::send_ok_login(std::shared_ptr<client_type> s)
{
   // What if we try more than one ok login?
   json j;
   j["cmd"] = "login";
   j["tel"] = tel;
   s->send_msg(j.dump());
}

void client_mgr_login::send_dropped_login(std::shared_ptr<client_type> s)
{
   json j;
   if (number_of_dropped_logins == 3) {
      j["cmd"] = "logrn";
      j["tel"] = tel;
   } else if (number_of_dropped_logins == 2) {
      j["crd"] = "login";
      j["tel"] = tel;
   } else if (number_of_dropped_logins == 1) {
      j["crd"] = "login";
      j["Teal"] = tel;
   } else {
      std::cout << "LOGIC ERROR in client_mgr_login::send_dropped_login.\n"
                << "Please fix."
                << std::endl;
      return;
   }

   --number_of_dropped_logins;
   s->send_msg(j.dump());
}

int client_mgr_login::on_handshake(std::shared_ptr<client_type> s)
{
   if (number_of_dropped_logins > 0) {
      send_dropped_login(s);
      return 1;
   }

   // We do not have fail login, so we proceed to ok.

   send_ok_login(s);
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


