#include "test_clients.hpp"

#include "menu.hpp"
#include "client_session.hpp"
#include "session_launcher.hpp"

namespace rt::cli
{

int replier::on_read( std::string msg
                    , std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);

   auto const cmd = j.at("cmd").get<std::string>();
   if (cmd == "login_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res != "ok")
         throw std::runtime_error("replier::login_ack");

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
         throw std::runtime_error("replier::subscribe_ack");

      return 1;
   }

   if (cmd == "publish") {
      auto items = j.at("items").get<std::vector<post>>();

      auto const f = [this, s](auto const& e)
      {
         talk_to(e.from, e.id, s);
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
      talk_to(from, post_id, s);
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
   throw std::runtime_error("replier::inexistent");
   return -1;
}

int replier::on_handshake(std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "login";
   j["user"] = op.user.id;
   j["password"] = op.user.pwd;
   j["menu_versions"] = std::vector<int> {};
   s->send_msg(j.dump());
   //std::cout << "Sending " << j.dump() << std::endl;
   return 1;
}

int replier::on_closed(boost::system::error_code ec)
{
   throw std::runtime_error("replier::on_closed");
   return -1;
};

void
replier::talk_to( std::string to
                , long long post_id
                , std::shared_ptr<client_type> s)
{
   //std::cout << "Sub: User " << op.user << " sending to " << to
   //          << ", post_id: " << post_id << std::endl;

   json j;
   j["cmd"] = "user_msg";
   j["from"] = op.user.id;
   j["to"] = to;
   j["msg"] = "Tenho interesse nesse carro, podemos conversar?";
   j["post_id"] = post_id;
   j["is_sender_post"] = false;

   s->send_msg(j.dump());
}

//_______________

int publisher::on_read(std::string msg, std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);

   auto const cmd = j.at("cmd").get<std::string>();
   if (cmd == "login_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res != "ok")
         throw std::runtime_error("publisher::login_ack");

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
         throw std::runtime_error("publisher::subscribe_ack");

      return send_group_msg(s);
   }

   if (cmd == "publish_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res != "ok")
         throw std::runtime_error("publisher::publish_ack");

      post_id = j.at("id").get<int>();
      //std::cout << op.user << " publish_ack " << post_id << std::endl;
      return handle_msg(s);
   }

   if (cmd == "publish") {
      // We are only interested in our own publishes at the moment.
      auto items = j.at("items").get<std::vector<post>>();

      auto cond = [this](auto const& e)
         { return e.from != op.user.id; };

      items.erase( std::remove_if( std::begin(items), std::end(items)
                                 , cond)
                 , std::end(items));

      // Ignores messages that are not our own.
      if (std::empty(items))
         return 1;

      if (std::size(items) != 1) {
         std::cout << std::size(items) << " " << msg << std::endl;
         throw std::runtime_error("publisher::publish1");
      }

      // Since we send only one publish at time items should contain
      // only one item now.

      if (post_id != items.front().id)
         throw std::runtime_error("publisher::publish2");

      //std::cout << op.user << " publish echo " << post_id << std::endl;
      server_echo = true;
      return handle_msg(s);
   }

   if (cmd == "user_msg") {
      // This test is to make sure the server is not sending us a
      // message meant to some other user.
      auto const to = j.at("to").get<std::string>();
      if (to != op.user.id)
         throw std::runtime_error("publisher::on_read5");

      auto const post_id2 = j.at("post_id").get<int>();
      if (post_id != post_id2) {
         std::cout << op.user << " " << post_id << " != " << post_id2 << " " << msg
                   << std::endl;
         throw std::runtime_error("publisher::on_read6");
      }

      //std::cout << op.user << " user_msg " << post_id << " "
      //          << user_msg_counter << std::endl;
      --user_msg_counter;
      return handle_msg(s);
   }

   throw std::runtime_error("publisher::on_read4");
   return -1;
}

int publisher::handle_msg(std::shared_ptr<client_type> s)
{
   if (server_echo && post_id != -1 && user_msg_counter == 0) {
      pub_stack.pop();
      if (std::empty(pub_stack)) {
         std::cout << "Pub: User " << op.user << " finished." << std::endl;
         return -1;
      }

      server_echo = false;
      post_id = -1;
      user_msg_counter = op.n_repliers;
      //std::cout << "=====> " << op.user << " " << post_id
      //          << " " << user_msg_counter <<  std::endl;
      return send_group_msg(s);
   }

   return 1;
}

int publisher::on_handshake(std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "login";
   j["user"] = op.user.id;
   j["password"] = op.user.pwd;
   j["menu_versions"] = std::vector<int> {};
   s->send_msg(j.dump());
   //std::cout << "Sending " << j.dump() << std::endl;
   return 1;
}

int publisher::on_closed(boost::system::error_code ec)
{
   throw std::runtime_error("publisher::on_closed");
   return -1;
};

int publisher::send_group_msg(std::shared_ptr<client_type> s) const
{
   //std::cout << "Pub: Stack size: " << std::size(pub_stack)
   //          << std::endl;

   post item {-1, op.user.id, "Not an interesting message."
                 , pub_stack.top()};
   json j_msg;
   j_msg["cmd"] = "publish";
   j_msg["items"] = std::vector<post>{item};
   s->send_msg(j_msg.dump());
   return 1;
}

//________________________________

int publisher2::on_read(std::string msg, std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);

   auto const cmd = j.at("cmd").get<std::string>();
   if (cmd == "login_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res != "ok")
         throw std::runtime_error("publisher2::login_ack");

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
         throw std::runtime_error("publisher2::publish_ack");

      auto const post_id = j.at("id").get<int>();
      post_ids.push_back(post_id);
      if (--msg_counter == 0)
         return -1;

      return 1;
   }

   throw std::runtime_error("publisher2::on_read4");
   return -1;
}

int publisher2::on_handshake(std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "login";
   j["user"] = op.user.id;
   j["password"] = op.user.pwd;
   j["menu_versions"] = std::vector<int> {};
   auto msg = j.dump();
   s->send_msg(msg);
   std::cout << "Sent: " << msg << std::endl;
   return 1;
}

int publisher2::on_closed(boost::system::error_code ec)
{
   throw std::runtime_error("publisher2::on_closed");
   return -1;
};

int publisher2::pub( menu_code_type const& pub_code
                 , std::shared_ptr<client_type> s) const
{
   post item {-1, op.user.id, "Not an interesting message."
                 , pub_code};
   json j_msg;
   j_msg["cmd"] = "publish";
   j_msg["items"] = std::vector<post>{item};
   s->send_msg(j_msg.dump());
   return 1;
}


//________________________________

int msg_pull::on_read(std::string msg, std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);

   auto const cmd = j.at("cmd").get<std::string>();
   if (cmd == "login_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res != "ok")
         throw std::runtime_error("msg_pull::login_ack");

      std::cout << "msg_pull::login_ack: ok." << std::endl;
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

   throw std::runtime_error("msg_pull::on_read4");
   return -1;
}

int msg_pull::on_handshake(std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "login";
   j["user"] = op.user.id;
   j["password"] = op.user.pwd;
   j["menu_versions"] = std::vector<int> {};
   auto msg = j.dump();
   s->send_msg(msg);
   std::cout << "Sent: " << msg << std::endl;
   return 1;
}

//________________________________

int register1::on_read(std::string msg, std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);

   auto const cmd = j.at("cmd").get<std::string>();
   if (cmd == "register_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res != "ok")
         throw std::runtime_error("register1::login_ack");

      op.user.id = j.at("id").get<std::string>();
      op.user.pwd = j.at("password").get<std::string>();
      std::cout << "Registered: " << op.user << std::endl;
      return -1;
   }

   throw std::runtime_error("register1::on_read2");
   return -1;
}

int register1::on_handshake(std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "register";
   auto msg = j.dump();
   s->send_msg(msg);
   return 1;
}

std::vector<login> test_reg(session_shell_cfg const& cfg, int n)
{
   boost::asio::io_context ioc;

   using client_type = register1;

   std::vector<login> logins {static_cast<std::size_t>(n)};
   launcher_cfg lcfg { logins, std::chrono::milliseconds {100}
                     , "Registration launching finished."};

   auto launcher =
      std::make_shared< session_launcher<client_type>
                      >( ioc
                       , register_cfg {}
                       , cfg
                       , lcfg);
   
   launcher->run({});
   ioc.run();

   auto const sessions = launcher->get_sessions();

   auto f = [](auto session)
      { return session->get_mgr().get_login(); };

   logins.clear();
   std::transform( std::cbegin(sessions), std::cend(sessions)
                 , std::back_inserter(logins), f);

   return logins;
}

}


