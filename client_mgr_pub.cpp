#include "client_mgr_pub.hpp"

#include "menu.hpp"
#include "client_session.hpp"

namespace rt
{

int client_mgr_pub::on_read(std::string msg, std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);

   auto const cmd = j.at("cmd").get<std::string>();
   if (cmd == "auth_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res != "ok")
         throw std::runtime_error("client_mgr_pub::auth_ack");

      auto const menus = j.at("menus").get<std::vector<menu_elem>>();
      auto const menu_codes = menu_elems_to_codes(menus);
      auto const pub_codes = channel_codes(menu_codes, menus);

      assert(!std::empty(pub_codes));

      auto pusher = [this](auto const& o)
         { pub_stack.push(o); };

      std::for_each(std::begin(pub_codes), std::end(pub_codes), pusher);

      json j_sub;
      j_sub["cmd"] = "subscribe";
      j_sub["channels"] = menu_codes;
      s->send_msg(j_sub.dump());
      return 1;
   }

   if (cmd == "subscribe_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res != "ok")
         throw std::runtime_error("client_mgr_pub::subscribe_ack");

      return send_group_msg(s);
   }

   if (cmd == "publish_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res != "ok")
         throw std::runtime_error("client_mgr_pub::publish_ack");

      post_id = j.at("id").get<int>();
      return handle_msg(s);
   }

   if (cmd == "publish") {
      // We are only interested in our own publishes at the moment.
      auto items = j.at("items").get<std::vector<pub_item>>();

      auto cond = [this](auto const& e)
         { return e.from != op.user; };

      items.erase( std::remove_if( std::begin(items), std::end(items)
                                 , cond)
                 , std::end(items));

      if (std::size(items) != 1)
         throw std::runtime_error("client_mgr_pub::publish1");

      // Since we send only one publish at time items should contain
      // only one item now.

      //if (post_id == items.front().id)
      //   throw std::runtime_error("client_mgr_pub::publish2");

      server_echo = true;
      return handle_msg(s);
   }

   if (cmd == "user_msg") {
      // This test is to make sure the server is not sending us a
      // message meant to some other user.
      auto const to = j.at("to").get<std::string>();
      if (to != op.user)
         throw std::runtime_error("client_mgr_pub::on_read5");

      // It looks like sometimes the user_msg comes before the
      // publish_ack is received, therefore we still do not have the
      // post_id and cannot assert it is the correct one.
      //auto const post_id = j.at("post_id").get<int>();

      --user_msg_counter;
      return handle_msg(s);
   }

   throw std::runtime_error("client_mgr_pub::on_read4");
   return -1;
}

int client_mgr_pub::handle_msg(std::shared_ptr<client_type> s)
{
   if (server_echo && post_id != -1 && user_msg_counter == 0) {
      pub_stack.pop();
      if (std::empty(pub_stack)) {
         std::cout << "Pub: User " << op.user << " finished." << std::endl;
         return -1;
      }

      server_echo = false;
      post_id = -1;
      user_msg_counter = op.n_listeners;
      return send_group_msg(s);
   }

   return 1;
}

int client_mgr_pub::on_handshake(std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "auth";
   j["from"] = op.user;
   j["menu_versions"] = std::vector<int> {};
   s->send_msg(j.dump());
   //std::cout << "Sending " << j.dump() << std::endl;
   return 1;
}

int client_mgr_pub::on_closed(boost::system::error_code ec)
{
   throw std::runtime_error("client_mgr_pub::on_closed");
   return -1;
};

int client_mgr_pub::send_group_msg(std::shared_ptr<client_type> s) const
{
   //std::cout << "Pub: Stack size: " << std::size(pub_stack)
   //          << std::endl;

   pub_item item {-1, op.user, "Not an interesting message."
                 , pub_stack.top()};
   json j_msg;
   j_msg["cmd"] = "publish";
   j_msg["items"] = std::vector<pub_item>{item};
   s->send_msg(j_msg.dump());
   return 1;
}

}

