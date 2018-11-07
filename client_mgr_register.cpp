#include "client_mgr_register.hpp"

#include "client_session.hpp"

namespace rt
{

client_mgr_register::client_mgr_register(cmgr_register_cf op_)
: op(op_)
{ }

int client_mgr_register::on_read( std::string msg
                             , std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);
   auto const cmd = j.at("cmd").get<std::string>();

   if (cmd != "register_ack") {
      std::cerr << "Server error. Please fix." << std::endl;
      throw std::runtime_error("client_mgr_register::on_read2");
      return op.on_read_ret;
   }

   auto res = j.at("result").get<std::string>();
   if (res == op.expected) {
      //std::cout << "Test register: ok." << std::endl;
      return op.on_read_ret;
   }

   std::cerr << "Test register: fail. Unexpected: " << cmd << std::endl;
   throw std::runtime_error("client_mgr_register::on_read2");
   return op.on_read_ret;
}

int client_mgr_register::on_handshake(std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "register";
   j["tel"] = op.user;
   s->send_msg(j.dump());
   return 1;
}

//___________________________________________________________________

client_mgr_register_typo::client_mgr_register_typo(std::string cmd_)
: cmd(cmd_)
{ }

int client_mgr_register_typo::on_read( std::string msg
                                  , std::shared_ptr<client_type> s)
{
   // A dropped register should not receive any message.
   std::cout << "Test register1: fail." << std::endl;
   throw std::runtime_error("client_mgr_register_typo::on_read");
   return -1;
}

int client_mgr_register_typo::on_closed(boost::system::error_code ec)
{
   //std::cout << "Test register1: ok." << std::endl;
   return -1;
}

int client_mgr_register_typo::on_handshake(std::shared_ptr<client_type> s)
{
   s->send_msg(cmd);
   return 1;
}

}

