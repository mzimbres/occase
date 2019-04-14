#include "client_mgr_pub.hpp"

#include "menu.hpp"
#include "utils.hpp"
#include "client_session.hpp"

namespace rt
{

int client_mgr_pub::on_read(std::string msg, std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);

   auto const cmd = j.at("cmd").get<std::string>();
   if (cmd == "login_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res != "ok")
         throw std::runtime_error("client_mgr_pub::login_ack");

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
      //std::cout << op.user << " publish_ack " << post_id << std::endl;
      return handle_msg(s);
   }

   if (cmd == "publish") {
      // We are only interested in our own publishes at the moment.
      auto items = j.at("items").get<std::vector<post>>();

      auto cond = [this](auto const& e)
         { return e.from != op.user; };

      items.erase( std::remove_if( std::begin(items), std::end(items)
                                 , cond)
                 , std::end(items));

      // Ignores messages that are not our own.
      if (std::empty(items))
         return 1;

      if (std::size(items) != 1) {
         std::cout << std::size(items) << " " << msg << std::endl;
         throw std::runtime_error("client_mgr_pub::publish1");
      }

      // Since we send only one publish at time items should contain
      // only one item now.

      if (post_id != items.front().id)
         throw std::runtime_error("client_mgr_pub::publish2");

      //std::cout << op.user << " publish echo " << post_id << std::endl;
      server_echo = true;
      return handle_msg(s);
   }

   if (cmd == "user_msg") {
      // This test is to make sure the server is not sending us a
      // message meant to some other user.
      auto const to = j.at("to").get<std::string>();
      if (to != op.user)
         throw std::runtime_error("client_mgr_pub::on_read5");

      auto const post_id2 = j.at("post_id").get<int>();
      if (post_id != post_id2) {
         std::cout << op.user << " " << post_id << " != " << post_id2 << " " << msg
                   << std::endl;
         throw std::runtime_error("client_mgr_pub::on_read6");
      }

      //std::cout << op.user << " user_msg " << post_id << " "
      //          << user_msg_counter << std::endl;
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
      //std::cout << "=====> " << op.user << " " << post_id
      //          << " " << user_msg_counter <<  std::endl;
      return send_group_msg(s);
   }

   return 1;
}

int client_mgr_pub::on_handshake(std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "login";
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

   post item {-1, op.user, "Not an interesting message."
                 , pub_stack.top()};
   json j_msg;
   j_msg["cmd"] = "publish";
   j_msg["items"] = std::vector<post>{item};
   s->send_msg(j_msg.dump());
   return 1;
}

//________________________________

int test_pub::on_read(std::string msg, std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);

   auto const cmd = j.at("cmd").get<std::string>();
   if (cmd == "login_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res != "ok")
         throw std::runtime_error("test_pub::login_ack");

      auto const menus = j.at("menus").get<std::vector<menu_elem>>();
      auto const menu_codes = menu_elems_to_codes(menus);
      auto const pub_codes = channel_codes(menu_codes, menus);

      assert(!std::empty(pub_codes));

      auto f = [s, this](auto const& o)
         { pub(o, s); };

      // Sending the publishes without waiting for the acks is the
      // expected way to use the server. At the moment however nothing
      // prevents it. The typical way of doing this it to put the
      // codes on a stack and send the next message as the ack of the
      // previous arrives.
      std::for_each( std::begin(pub_codes)
                   , std::end(pub_codes)
                   , f);

      msg_counter = ssize(pub_codes);;
      return 1;
   }

   if (cmd == "publish_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res != "ok")
         throw std::runtime_error("test_pub::publish_ack");

      auto const post_id = j.at("id").get<int>();
      post_ids.push_back(post_id);
      if (--msg_counter == 0)
         return -1;

      return 1;
   }

   throw std::runtime_error("test_pub::on_read4");
   return -1;
}

int test_pub::on_handshake(std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "login";
   j["from"] = op.user;
   j["menu_versions"] = std::vector<int> {};
   auto msg = j.dump();
   s->send_msg(msg);
   std::cout << "Sent: " << msg << std::endl;
   return 1;
}

int test_pub::on_closed(boost::system::error_code ec)
{
   throw std::runtime_error("test_pub::on_closed");
   return -1;
};

int test_pub::pub( menu_code_type const& pub_code
                 , std::shared_ptr<client_type> s) const
{
   post item {-1, op.user, "Not an interesting message."
                 , pub_code};
   json j_msg;
   j_msg["cmd"] = "publish";
   j_msg["items"] = std::vector<post>{item};
   s->send_msg(j_msg.dump());
   return 1;
}


//________________________________

int test_msg_pull::on_read(std::string msg, std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);

   auto const cmd = j.at("cmd").get<std::string>();
   if (cmd == "login_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res != "ok")
         throw std::runtime_error("test_msg_pull::login_ack");

      std::cout << "test_msg_pull::login_ack: ok." << std::endl;
      return 1;
   }

   if (cmd == "user_msg") {
      auto const post_id = j.at("post_id").get<int>();
      post_ids.push_back(post_id);
      //std::cout << "Expecting: " << op.expected_user_msgs << std::endl;
      if (--op.expected_user_msgs == 0)
         return -1;
      return 1;
   }

   throw std::runtime_error("test_msg_pull::on_read4");
   return -1;
}

int test_msg_pull::on_handshake(std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "login";
   j["from"] = op.user;
   j["menu_versions"] = std::vector<int> {};
   auto msg = j.dump();
   s->send_msg(msg);
   std::cout << "Sent: " << msg << std::endl;
   return 1;
}

//________________________________

int test_register::on_read(std::string msg, std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);

   auto const cmd = j.at("cmd").get<std::string>();
   if (cmd == "register_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res != "ok")
         throw std::runtime_error("test_register::login_ack");

      op.user = j.at("id").get<std::string>();
      pwd = j.at("password").get<std::string>();
      return -1;
   }

   throw std::runtime_error("test_register::on_read2");
   return -1;
}

int test_register::on_handshake(std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "register";
   auto msg = j.dump();
   s->send_msg(msg);
   std::cout << "Sent: " << msg << std::endl;
   return 1;
}

}

