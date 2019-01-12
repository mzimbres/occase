#include "client_mgr_gmsg_check.hpp"

#include "menu.hpp"
#include "client_session.hpp"

namespace rt
{

int client_mgr_gmsg_check::on_read( std::string msg
                                  , std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);
   //std::cout << j << std::endl;
   auto const cmd = j.at("cmd").get<std::string>();

   if (cmd == "auth_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res == "ok") {
         auto const menus = j.at("menus").get<std::vector<menu_elem>>();
         auto const channels = get_hashes(menus.front().data, 2);
         if (std::empty(channels))
            throw std::runtime_error("client_mgr_gmsg_check::on_read0");
         tot_msgs = op.n_publishers * std::size(channels)
                  * op.msgs_per_channel_per_user;
         std::cout << op.user << " expects: " << tot_msgs << std::endl;
         json j_sub;
         j_sub["cmd"] = "subscribe";
         j_sub["channels"] = channels;
         s->send_msg(j_sub.dump());

         for (auto const& e : channels)
            counters.insert({e, {0, false, false}});

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
      auto const from = j.at("from").get<std::string>();
      auto const to = j.at("to").get<std::string>();
      //auto const id = j.at("id").get<int>();

      //std::cout << "from " << from << ", id " << id << std::endl;
      auto const match = counters.find(to);
      if (match == std::end(counters))
         throw std::runtime_error("client_mgr_gmsg_check::on_read5");

      if (match->second.acked)
         throw std::runtime_error("client_mgr_gmsg_check::on_read6");

      ++match->second.counter;
      auto const per_ch_msgs = op.msgs_per_channel_per_user * op.n_publishers;
      if (match->second.counter > per_ch_msgs) {
         // Unsubscribe sent but unprocessed by the server.
         if (!match->second.sent)
            throw std::runtime_error("client_mgr_gmsg_check::on_read10");

      } else if (match->second.counter < per_ch_msgs) {
         --tot_msgs;
      } else {
         json j_unsub;
         j_unsub["cmd"] = "unsubscribe";
         j_unsub["channels"] = std::vector<std::string>{to};
         // This will unsubscribe one by one. We still have to test
         // unsubscription fom many channels at once.
         s->send_msg(j_unsub.dump());
         --tot_msgs;
         match->second.sent = true;
      }

      return 1;
   }

   if (cmd == "unsubscribe_ack") {
      auto const to = j.at("channels").get<std::vector<std::string>>();
      // Since we are sending unsubscribs one at a time it is ok to
      // take only the front.
      auto const match = counters.find(to.front());
      if (match == std::end(counters))
         throw std::runtime_error("client_mgr_gmsg_check::on_read8");

      match->second.acked = true;
      if (tot_msgs == 0) {
         // Check that all counters are correct.
         for (auto const& o : counters)
            if (!o.second.acked && !o.second.sent) {
               std::cout << o.second.acked << " " << o.second.sent
                         << std::endl;
               throw std::runtime_error("client_mgr_gmsg_check::on_read7");
            }

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
   j["version"] = -1;
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

