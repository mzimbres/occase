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
         menus = j.at("menus").get<std::vector<menu_elem>>();
         auto const hash_codes = menu_elems_to_codes(menus);
         auto const arrays = channel_codes(hash_codes, menus);
         std::vector<std::string> comb_codes;

         std::transform( std::begin(arrays), std::end(arrays)
                       , std::back_inserter(comb_codes)
                       , [this](auto const& o) {
                         return convert_to_hash_code(o, menus);});

         if (std::empty(comb_codes))
            throw std::runtime_error("client_mgr_gmsg_check::on_read0");

         tot_msgs = op.n_publishers * std::size(comb_codes)
                  * op.msgs_per_channel_per_user;

         std::cout << op.user << " expects: " << tot_msgs << std::endl;
         json j_sub;
         j_sub["cmd"] = "subscribe";
         j_sub["channels"] = hash_codes;
         s->send_msg(j_sub.dump());

         for (auto const& e : comb_codes)
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
      auto const id = j.at("id").get<long long>();
      auto const from = j.at("from").get<std::string>();
      auto const to =
         j.at("to").get<std::vector<std::vector<std::vector<int>>>>();
      auto const code = convert_to_hash_code(to, menus);

      auto const match = counters.find(code);
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
         j_unsub["channels"] =
            std::vector<std::vector<std::vector<int>>>{to};
         // This will unsubscribe one by one. We still have to test
         // unsubscription fom many channels at once.
         s->send_msg(j_unsub.dump());
         --tot_msgs;
         match->second.sent = true;
      }

      speak_to_publisher(from, id, s);
      return 1;
   }

   if (cmd == "unsubscribe_ack") {
      if (tot_msgs == 0) {
         // TODO: count the acks.
         std::cout << "Test finished for user: " << op.user << std::endl;
         return -1;
      }

      return 1;
   }

   if (cmd == "user_msg_server_ack") {
      std::cout << op.user << ": Ack received" << std::endl;
      return 1;
   }

   if (cmd == "user_msg") {
      std::cout << msg << std::endl;
      auto const post_id = j.at("post_id").get<long long>();
      auto const from = j.at("from").get<std::string>();
      speak_to_publisher(from, post_id, s);
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
   j["menu_versions"] = std::vector<int> {};
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

void
client_mgr_gmsg_check::speak_to_publisher(
      std::string to, long long post_id, std::shared_ptr<client_type> s)
{
   std::cout << "Message to: " << to << ", post_id: " << post_id << std::endl;
   json j;
   j["cmd"] = "user_msg";
   j["from"] = op.user;
   j["to"] = to;
   j["msg"] = "Tenho interesse nesse carro, podemos conversar?";
   j["post_id"] = post_id;
   j["is_sender_post"] = false;

   s->send_msg(j.dump());
}

}

