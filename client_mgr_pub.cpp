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

      auto const pusher = [this](auto const& o)
      { pub_stack.push({false, -1, o}); };

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

      auto const id = j.at("id").get<long long>();
      pub_stack.top().post_id = id;
      //std::cout << "Pub: Received publish ack. Post id: " << id << std::endl;
      user_msg_counter = op.n_listeners;
      return 1;
   }

   if (cmd == "publish") {
      // We are only interested in our own publishes for the moment.
      auto const from = j.at("from").get<std::string>();
      if (from != op.user)
         return 1;

      auto const id = j.at("id").get<long long>();
      assert(pub_stack.top().post_id == id);

      pub_stack.top().server_echo = true;
      return 1;
   }

   if (cmd == "user_msg") {
      auto const from = j.at("from").get<std::string>();
      auto const to = j.at("to").get<std::string>();
      auto const post_id = j.at("post_id").get<long long>();

      //std::cout << "Pub: Received user_msg from " << from 
      //          << " ===> " << user_msg_counter
      //          << " post_id: " << post_id << std::endl;

      assert(to == op.user);
      assert(pub_stack.top().post_id == post_id);

      // This assert is not strictly necessary but it would be strange
      // to receive a user message before the server echoed the
      // publish command back.
      assert(pub_stack.top().server_echo);


      if (--user_msg_counter == 0) {
         pub_stack.pop();
         if (std::empty(pub_stack)) {
            std::cout << "Pub: User " << op.user << " finished." << std::endl;
            return -1;
         }

         return send_group_msg(s);
      }

      return 1; // Wait for further user messages.
   }

   throw std::runtime_error("client_mgr_pub::on_read4");
   return -1;
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
   json j_msg;
   j_msg["cmd"] = "publish";
   j_msg["from"] = op.user;
   j_msg["to"] = pub_stack.top().pub_code;
   j_msg["msg"] = "Not an interesting message.";
   s->send_msg(j_msg.dump());
   return 1;
}

}

