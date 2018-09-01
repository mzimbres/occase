#include "client_mgr_login.hpp"

#include "client_session.hpp"

client_mgr_login::client_mgr_login(std::string tel_)
: tel(tel_)
{ }

int client_mgr_login::on_read(json j, std::shared_ptr<client_type> s)
{
   auto cmd = j["cmd"].get<std::string>();

   if (cmd == "login_ack")
      return on_login_ack(std::move(j), s);

   std::cout << "Unknown command." << std::endl;
   std::cerr << "Server error. Please fix." << std::endl;

   return -1;
}

int client_mgr_login::on_fail_read(boost::system::error_code ec)
{
   return 1;
}

int client_mgr_login::on_ok_login_ack( json j
                                     , std::shared_ptr<client_type> s)
{
   std::cout << "login_ack: ok." << std::endl;

   if (--number_of_ok_logins == 0) {
      std::cout << "Test login: ok." << std::endl;
      return -1;
   }

   std::cout << "Test login: fail." << std::endl;
   return -1;
}

int client_mgr_login::on_login_ack(json j, std::shared_ptr<client_type> s)
{
   auto res = j["result"].get<std::string>();
   if (res == "ok")
      return on_ok_login_ack(std::move(j), s);

   // We still have not fail login server ack.
   std::cout << "SERVER ERROR: client_mgr_login::on_login_ack.\n"
             << "Please report." << std::endl;
   return 1;
}

void client_mgr_login::send_ok_login(std::shared_ptr<client_type> s)
{
   // What if we try more than one ok login?
   json j;
   j["cmd"] = "login";
   j["tel"] = tel;
   send_msg(j.dump(), s);
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
   send_msg(j.dump(), s);
}

void client_mgr_login::send_msg( std::string msg
                               , std::shared_ptr<client_type> s)
{
   auto is_empty = std::empty(msg_queue);
   msg_queue.push(std::move(msg));

   if (is_empty)
      s->write(msg_queue.front());
}

int client_mgr_login::on_write(std::shared_ptr<client_type> s)
{
   //std::cout << "on_write" << std::endl;

   msg_queue.pop();
   if (msg_queue.empty())
      return 1; // No more message to send to the client.

   s->write(msg_queue.front());

   return 1;
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

