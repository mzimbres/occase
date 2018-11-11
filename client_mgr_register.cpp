#include "client_mgr_register.hpp"

#include "client_session.hpp"

namespace rt
{

int client_mgr_register::on_read( std::string msg
                             , std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);
   auto const cmd = j.at("cmd").get<std::string>();

   if (cmd != "register_ack") {
      throw std::runtime_error("client_mgr_register::on_read2");
      return op.on_read_ret;
   }

   auto res = j.at("result").get<std::string>();
   if (res == op.expected) {
      //std::cout << "Test register: ok." << std::endl;
      return op.on_read_ret;
   }

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

int client_mgr_register_typo::
on_handshake(std::shared_ptr<client_type> s) const
{
   s->send_msg(cmd);
   return 1;
}

}

