#include "client_mgr_login.hpp"

#include "client_session.hpp"

namespace rt
{

client_mgr_login::client_mgr_login(cmgr_login_cf op_)
: op(op_)
{ }

int client_mgr_login::on_read( std::string msg
                             , std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);
   auto const cmd = j.at("cmd").get<std::string>();

   if (cmd != "login_ack") {
      std::cerr << "Server error. Please fix." << std::endl;
      throw std::runtime_error("client_mgr_login::on_read2");
      return op.on_read_ret;
   }

   auto res = j.at("result").get<std::string>();
   if (res == op.expected) {
      //std::cout << "Test login: ok." << std::endl;
      return op.on_read_ret;
   }

   std::cerr << "Test login: fail. Unexpected: " << cmd << std::endl;
   throw std::runtime_error("client_mgr_login::on_read2");
   return op.on_read_ret;
}

int client_mgr_login::on_handshake(std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "login";
   j["tel"] = op.user;
   s->send_msg(j.dump());
   return 1;
}

//___________________________________________________________________

client_mgr_login_typo::client_mgr_login_typo(std::string cmd_)
: cmd(cmd_)
{ }

int client_mgr_login_typo::on_read( std::string msg
                                  , std::shared_ptr<client_type> s)
{
   // A dropped login should not receive any message.
   std::cout << "Test login1: fail." << std::endl;
   throw std::runtime_error("client_mgr_login_typo::on_read");
   return -1;
}

int client_mgr_login_typo::on_closed(boost::system::error_code ec)
{
   //std::cout << "Test login1: ok." << std::endl;
   return -1;
}

int client_mgr_login_typo::on_handshake(std::shared_ptr<client_type> s)
{
   s->send_msg(cmd);
   return 1;
}

}

