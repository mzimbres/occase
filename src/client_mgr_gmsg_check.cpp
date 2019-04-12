#include "client_mgr_gmsg_check.hpp"

#include "menu.hpp"
#include "client_session.hpp"

namespace rt
{

int client_mgr_gmsg_check::on_read( std::string msg
                                  , std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);

   auto const cmd = j.at("cmd").get<std::string>();
   if (cmd == "auth_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res != "ok")
         throw std::runtime_error("client_mgr_gmsg_check::auth_ack");

      menus = j.at("menus").get<std::vector<menu_elem>>();
      auto const menu_codes = menu_elems_to_codes(menus);
      auto const pub_codes = channel_codes(menu_codes, menus);
      assert(!std::empty(pub_codes));

      to_receive_posts = op.n_publishers * std::size(pub_codes);

      //std::cout << "Sub: User " << op.user << " expects: " << to_receive_posts
      //          << std::endl;

      json j_sub;
      j_sub["cmd"] = "subscribe";
      j_sub["channels"] = menu_codes;
      s->send_msg(j_sub.dump());
      return 1;
   }

   if (cmd == "subscribe_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res != "ok")
         throw std::runtime_error("client_mgr_gmsg_check::subscribe_ack");

      return 1;
   }

   if (cmd == "publish") {
      auto items = j.at("items").get<std::vector<post>>();

      auto const f = [this, s](auto const& e)
      {
         speak_to_publisher(e.from, e.id, s);
      };

      std::for_each(std::begin(items), std::end(items), f);

      return 1;
   }

   if (cmd == "user_msg") {
      // Used only by the app. Not in the tests.
      std::cout << "Should not get here in the tests." << std::endl;
      std::cout << msg << std::endl;
      auto const post_id = j.at("post_id").get<long long>();
      auto const from = j.at("from").get<std::string>();
      speak_to_publisher(from, post_id, s);
      return 1;
   }

   if (cmd == "user_msg_server_ack") {
      if (--to_receive_posts == 0) {
         //std::cout << "Sub: User " << op.user << " finished." << std::endl;
         return -1; // Done.
      }

      return 1; // Fix the server and remove this.
   }

   std::cout << j << std::endl;
   throw std::runtime_error("client_mgr_gmsg_check::inexistent");
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
   throw std::runtime_error("client_mgr_gmsg_check::on_closed");
   return -1;
};

void
client_mgr_gmsg_check::speak_to_publisher(
      std::string to, long long post_id, std::shared_ptr<client_type> s)
{
   //std::cout << "Sub: User " << op.user << " sending to " << to
   //          << ", post_id: " << post_id << std::endl;

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

