#include "client_mgr_gmsg_check.hpp"

#include "menu_parser.hpp"
#include "client_session.hpp"

namespace rt
{

client_mgr_gmsg_check::client_mgr_gmsg_check(options_type op_)
: op(op_)
{
}

int client_mgr_gmsg_check::on_read( std::string msg
                                  , std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);
   //std::cout << j << std::endl;
   auto const cmd = j.at("cmd").get<std::string>();

   if (cmd == "auth_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res == "ok") {
         auto const menu_str = j.at("menu").get<std::string>();
         auto const jmenu = json::parse(menu_str);
         auto const channels = get_hashes(jmenu);
         tot_msgs = op.n_publishers * std::size(channels)
                  * op.msgs_per_channel;
         std::cout << op.user << " expects: " << tot_msgs << std::endl;
         json j_sub;
         j_sub["cmd"] = "subscribe";
         j_sub["channels"] = channels;
         s->send_msg(j_sub.dump());
         return 1;
      }

      throw std::runtime_error("client_mgr_gmsg_check::on_read1");
      return -1;
   }

   if (cmd == "subscribe_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res != "ok")
         throw std::runtime_error("client_mgr_gmsg_check::on_read2");
      return 1;
   }

   if (cmd == "publish") {
      //auto const body = j.at("msg").get<std::string>();
      //std::cout << "Group msg check: " << body << std::endl;
      //std::cout << j << std::endl;
      //auto const from = j.at("from").get<std::string>();
      //std::cout << from << " != " << op.user << std::endl;
      --tot_msgs;
      //std::cout << "Remaining msgs for user " << op.user << ": "
      //          << tot_msgs << std::endl;
      if (tot_msgs == 0) {
         std::cout << "Test finished for user: " << op.user << std::endl;
         return -1;
      }
      return 1;
   }

   std::cout << j << std::endl;
   throw std::runtime_error("client_mgr_gmsg_check::on_read4");
   return -1;
}

int client_mgr_gmsg_check::on_handshake(std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "auth";
   j["from"] = op.user;
   j["menu_version"] = -1;
   s->send_msg(j.dump());
   //std::cout << "Sending " << j.dump() << std::endl;
   return 1;
}

int client_mgr_gmsg_check::on_closed(boost::system::error_code ec)
{
   std::cout << "Test sim: fail." << std::endl;
   throw std::runtime_error("client_mgr_gmsg_check::on_closed");
   return -1;
};

}

