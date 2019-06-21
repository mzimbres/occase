#include "test_clients.hpp"

#include "menu.hpp"
#include "client_session.hpp"
#include "session_launcher.hpp"

namespace
{

std::vector<std::string> const nicks
{ "Magraboia"
, "Jujuba"
, "Caju"
, "Lasca Fina"
, "Kabuff"
, "Gomes"
, "Fui lá"
, "Felino"
, "Zé Caque"
, "Noski"
, "Bezerro"
, "Palminha"
, "Rebolo"
, "Fui e Voltei"
, "Mijoca"
, "Brancona"
, "Mandibula"
, "Largado"
, "Mosga"
, "Victor"
, "Peste Branda"
};

std::string make_login_cmd(rt::cli::login const& user)
{
   json j;
   j["cmd"] = "login";
   j["user"] = user.id;
   j["password"] = user.pwd;
   j["menu_versions"] = std::vector<int> {};
   return j.dump();
}

std::string
make_post_cmd(rt::menu_code_type const& menu_code)
{
   rt::post item { -1, {}, "Not an interesting message."
                 , "Kabuff", menu_code, 0, 0};
   json j;
   j["cmd"] = "publish";
   j["items"] = std::vector<rt::post>{item};
   return j.dump();
}

}

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
      j_sub["last_post_id"] = 0;
      j_sub["channels"] = menu_codes;
      j_sub["filter"] = 0;
      s->send_msg(j_sub.dump());
      return 1;
   }

   if (cmd == "subscribe_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res != "ok")
         throw std::runtime_error("replier::subscribe_ack");

      return 1;
   }

   if (cmd == "post") {
      auto items = j.at("items").get<std::vector<post>>();

      auto const f = [this, s](auto const& e)
         { send_chat_msg(e.from, e.id, s); };

      std::for_each(std::begin(items), std::end(items), f);

      return 1;
   }

   if (cmd == "message") {
      auto const type = j.at("type").get<std::string>();
      if (type == "server_ack") {
         if (--to_receive_posts == 0) {
            std::cout << "User " << op.user << " done. (Replier)."
                      << std::endl;
            return -1; // Done.
         }

         return 1; // Fix the server and remove this.
      }

      if (type == "chat") {
         // Used only by the app. Not in the tests.
         std::cout << "Should not get here in the tests." << std::endl;
         std::cout << msg << std::endl;
         auto const post_id = j.at("post_id").get<long long>();
         auto const from = j.at("from").get<std::string>();

         ack_chat(from, post_id, s, "app_ack_received");
         ack_chat(from, post_id, s, "app_ack_read");
         send_chat_msg(from, post_id, s);
         return 1;
      }

      // Currently we make no use of these commands.
      if (type == "app_ack_received") {
         return 1;
      }

      if (type == "app_ack_read") {
         return 1;
      }
   }

   std::cout << j << std::endl;
   throw std::runtime_error("replier::inexistent");
   return -1;
}

int replier::on_handshake(std::shared_ptr<client_type> s)
{
   auto const str = make_login_cmd(op.user);
   s->send_msg(str);
   return 1;
}

int replier::on_closed(boost::system::error_code ec)
{
   throw std::runtime_error("replier::on_closed");
   return -1;
};

void
replier::send_chat_msg( std::string to, long long post_id
                      , std::shared_ptr<client_type> s)
{
   //std::cout << "Sub: User " << op.user << " sending to " << to
   //          << ", post_id: " << post_id << std::endl;

   auto const n = std::stoi(op.user.id);
   json j;
   j["cmd"] = "message";
   j["type"] = "chat";
   j["to"] = to;
   j["msg"] = "Tenho interesse nesse carro, podemos conversar?";
   j["post_id"] = post_id;
   j["is_sender_post"] = false;
   j["nick"] = nicks.at(n % std::size(nicks));

   s->send_msg(j.dump());
}

void
replier::ack_chat( std::string to, long long post_id
                 , std::shared_ptr<client_type> s
                 , std::string const& type)
{
   json j;
   j["cmd"] = "message";
   j["type"] = type;
   j["to"] = to;
   j["post_id"] = post_id;
   j["is_sender_post"] = false;
   j["nick"] = "Zebu";

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
      j_sub["last_post_id"] = 0;
      j_sub["channels"] = menu_codes;
      j_sub["filter"] = 0;
      s->send_msg(j_sub.dump());
      return 1;
   }

   if (cmd == "subscribe_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res != "ok")
         throw std::runtime_error("publisher::subscribe_ack");

      return send_post(s);
   }

   if (cmd == "publish_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res != "ok")
         throw std::runtime_error("publisher::publish_ack");

      post_id = j.at("id").get<int>();
      //std::cout << op.user << " publish_ack " << post_id << std::endl;
      return handle_msg(s);
   }

   if (cmd == "post") {
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

   if (cmd == "message") {
      auto const type = j.at("type").get<std::string>();
      if (type == "server_ack")
         return 1;

      if (type == "chat") {
         // This test is to make sure the server is not sending us a
         // message meant to some other user.
         auto const to = j.at("to").get<std::string>();
         if (to != op.user.id)
            throw std::runtime_error("publisher::on_read5");

         auto const post_id2 = j.at("post_id").get<int>();
         if (post_id != post_id2) {
            std::cout << op.user << " " << post_id << " != " << post_id2
                      << " " << msg << std::endl;
            throw std::runtime_error("publisher::on_read6");
         }

         //std::cout << op.user << " message " << post_id << " "
         //          << user_msg_counter << std::endl;
         --user_msg_counter;
         return handle_msg(s);
      }
   }

   std::cout << "Error: publisher ===> " << cmd << std::endl;
   throw std::runtime_error("publisher::on_read4");
   return -1;
}

int publisher::handle_msg(std::shared_ptr<client_type> s)
{
   if (server_echo && post_id != -1 && user_msg_counter == 0) {
      pub_stack.pop();
      if (std::empty(pub_stack)) {
         std::cout << "User " << op.user << " ok. (Publisher)."
                   << std::endl;
         return -1;
      }

      server_echo = false;
      post_id = -1;
      user_msg_counter = op.n_repliers;
      //std::cout << "=====> " << op.user << " " << post_id
      //          << " " << user_msg_counter <<  std::endl;
      return send_post(s);
   }

   return 1;
}

int publisher::on_handshake(std::shared_ptr<client_type> s)
{
   auto const str = make_login_cmd(op.user);
   s->send_msg(str);
   return 1;
}

int publisher::on_closed(boost::system::error_code ec)
{
   throw std::runtime_error("publisher::on_closed");
   return -1;
};

int publisher::send_post(std::shared_ptr<client_type> s) const
{
   auto const str = make_post_cmd(pub_stack.top());
   s->send_msg(str);
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
      if (--msg_counter == 0) {
         std::cout << "User " << op.user << " ok. (Publisher2)."
                   << std::endl;
         return -1;
      }

      return 1;
   }

   throw std::runtime_error("publisher2::on_read4");
   return -1;
}

int publisher2::on_handshake(std::shared_ptr<client_type> s)
{
   auto const str = make_login_cmd(op.user);
   s->send_msg(str);
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
             , "Campeao", pub_code, 0, 0};
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

      return 1;
   }

   if (cmd == "message") {
      auto const type = j.at("type").get<std::string>();
      if (type == "server_ack") {
         return 1;
      }

      if (type == "chat") {
         auto const post_id = j.at("post_id").get<int>();
         post_ids.push_back(post_id);
         //std::cout << "Expecting: " << op.expected_user_msgs
         //          << std::endl;
         if (--op.expected_user_msgs == 0) {
            std::cout << "User " << op.user << " ok. (msg_pull)."
                      << std::endl;
            return -1;
         }
         return 1;
      }
   }

   if (cmd == "publish_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res != "ok")
         throw std::runtime_error("msg_pull::publish_ack");

      //auto const post_id = j.at("id").get<int>();
      //std::cout << op.user << " publish_ack " << post_id << std::endl;
      return -1;
   }

   throw std::runtime_error("msg_pull::on_read4");
   return -1;
}

int msg_pull::on_handshake(std::shared_ptr<client_type> s)
{
   auto const str = make_login_cmd(op.user);
   s->send_msg(str);
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
      std::cout << "User " << op.user << " ok. (register1)."
                << std::endl;
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

//________________________________

int login_err::on_read(std::string msg, std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);

   auto const cmd = j.at("cmd").get<std::string>();
   if (cmd == "login_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res != "fail")
         throw std::runtime_error("login_err::login_ack");

      return 1;
   }

   std::cout << "User " << op.user << " ok. (login_err)."
             << std::endl;
   throw std::runtime_error("login_err::on_read");
   return -1;
}

int login_err::on_handshake(std::shared_ptr<client_type> s)
{
   auto str = make_login_cmd(op.user);
   s->send_msg(str);
   return 1;
}

//________________________________

int early_close::on_read(std::string msg, std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);

   auto const cmd = j.at("cmd").get<std::string>();
   if (cmd == "login_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res != "ok")
         throw std::runtime_error("early_close::login_ack");

      send_post(s);
      return 1;
   }

   std::cout << "early_close: " << msg << std::endl;

   throw std::runtime_error("early_close::on_read4");
   return -1;
}

int early_close::on_handshake(std::shared_ptr<client_type> s)
{
   auto const str = make_login_cmd(op.user);
   s->send_msg(str);
   return 1;
}

void early_close::send_post(std::shared_ptr<client_type> s) const
{
   auto const code = code_type {{{0, 0, 0, 0}}, {{0, 0, 0, 0}}};
   auto const str = make_post_cmd(code);
   s->send_msg(str, -1);
}

//________________________________

std::vector<login> test_reg(session_shell_cfg const& cfg, int n)
{
   boost::asio::io_context ioc;

   using client_type = register1;
   using config_type = client_type::options_type;

   std::vector<login> logins {static_cast<std::size_t>(n)};
   launcher_cfg lcfg {logins, std::chrono::milliseconds {100}, ""};

   auto launcher =
      std::make_shared< session_launcher<client_type>
                      >( ioc
                       , config_type {}
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

