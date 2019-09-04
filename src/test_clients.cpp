#include "test_clients.hpp"

#include "menu.hpp"
#include "client_session.hpp"
#include "session_launcher.hpp"

namespace rt
{

std::vector<std::string> const nicks
{ "Landau"
, "Feynman"
, "Dirac"
, "Weinberg"
, "Planck"
, "Schrödinger"
, "Euler"
, "Gauß"
, "Lattes"
, "Maxwell"
, "Newton"
, "Leibniz"
, "Ampére"
, "De Broglie"
, "Born"
, "Minkowski"
, "Glaschow"
, "Salam"
, "Gell Mann"
, "Tomonaga"
, "Schwinger"
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
   json sub;
   sub["msg"] = "Not an interesting message.";
   sub["nick"] = "Wheeler";
   sub["avatar"] = "9ac8316ca55e6d888d43092fd73a78d6";

   rt::post item {-1, {}, sub.dump(), menu_code, 0, 0};
   json j;
   j["cmd"] = "publish";
   j["items"] = std::vector<rt::post>{item};
   return j.dump();
}


/* Collects the channel codes from all menus and returns the array
 * containing them. The ouput array is described in the documentation
 * of channel_codes below.
 */
auto create_channels(menu_elems_array_type const& menus)
{
   auto const c0 = menu_elems_to_codes(menus.at(0));
   auto const c1 = menu_elems_to_codes(menus.at(1));
   return menu_code_type {c0, c1};
}

auto create_channels2(menu_elems_array_type const& menus)
{
   auto const max = std::numeric_limits<int>::max();
   auto const c0 = menu_elems_to_codes(menus.at(0));
   auto const c1 = menu_elems_to_codes(menus.at(1));
   auto const r0 = menu_to_channel_codes(c0, menus.at(0).depth, max);
   auto const r1 = menu_to_channel_codes(c1, menus.at(1).depth, max);
   return menu_code_type2 {r0, r1};
}

/* This function receives input in the form
 *
 *   _________menu_1__________     __________menu_2________   etc.
 *  |                         |   |                        |
 * [[[1, 2, 3], [3, 4, 1], ...], [[a, b, c], [d, e, f], ...]]
 *
 * The menu_1 may refer to regions of a country and menu_2 to a
 * product for example.  The inner most array contains the coordinates
 * of the menu items the user wants to subscribe to. It has the form
 *
 *    [1, 2, 3, ..]
 *
 * Each position in this array refers to a level in the menu, for
 * example
 *
 *    [State, City, Neighbourhood]
 *
 * This array is contained in another array with the other channels
 * the user wants to subscribe to, for example
 *
 *    [Sao Paulo, Atibaia, Vila Santista]
 *    [Sao Paulo, Atibaia, Centro]
 *    [Sao Paulo, Campinas, Barao Geraldo]
 *    ...
 *
 * These arrays in turn are grouped in the outer most array where each
 * element corresponds to a menu. There will be typically two or three
 * menus per app.
 * 
 * The function respects the menu depth, so if the menu coodinates
 * have length 6 but the the hash codes are generate for depth 2 only
 * the first two elements will be considered.
 *
 * The output is the combination of the codes respecting the depths.
 * For the input array above the output would be
 *
 * [[[[1, 2], [a, b]]], [[[1, 2], [c, d]]], ..., [[[3, 4], [a, b]]], ...
 *
 * Each element of the outermost array will have length one.
 */
std::vector<menu_code_type>
channel_codes( menu_code_type const& channels
             , menu_elems_array_type const& menus)
{
   std::vector<menu_code_type> ret;
   for (auto i = 0; i < ssize(channels.at(0)); ++i) {
      for (auto j = 0; j < ssize(channels.at(1)); ++j) {
         auto begin0 = std::begin(channels.at(0).at(i));
         auto end0 = begin0 + menus.at(0).depth;

         auto begin1 = std::begin(channels.at(1).at(j));
         auto end1 = begin1 + menus.at(1).depth;

         channel_code_type c0 {begin0, end0};
         channel_code_type c1 {begin1, end1};

         menu_channel_elem_type d0 {c0};
         menu_channel_elem_type d1 {c1};

         menu_code_type foo {d0, d1};
         ret.push_back(foo);
      }
   }

   return ret;
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

      menus = j.at("menus").get<menu_elems_array_type>();
      assert(std::size(menus) == 2);

      auto const menu_codes_0 = menu_elems_to_codes(menus.at(0));
      auto const menu_codes_1 = menu_elems_to_codes(menus.at(1));

      to_receive_posts = op.n_publishers
                       * std::size(menu_codes_0)
                       * std::size(menu_codes_1);

      menu_code_type menu_codes {menu_codes_0, menu_codes_1};

      //std::cout << "Sub: User " << op.user << " expects: " << to_receive_posts
      //          << std::endl;

      json j_sub;
      j_sub["cmd"] = "subscribe";
      j_sub["last_post_id"] = 0;
      j_sub["channels"] = create_channels2(menus);
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

      if (type == "chat" || type == "chat_redirected") {
         std::cout << "Should not get here in the tests." << std::endl;
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
   j["refers_to"] = -1;
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

//_______________________________________

int
leave_after_n_posts::on_read( std::string msg
                            , std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);

   auto const cmd = j.at("cmd").get<std::string>();
   if (cmd == "login_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res != "ok")
         throw std::runtime_error("leave_after_n_posts::login_ack");

      json j_sub;
      j_sub["cmd"] = "subscribe";
      j_sub["last_post_id"] = 0;
      j_sub["channels"] = menu_code_type2 {};
      j_sub["filter"] = 0;
      s->send_msg(j_sub.dump());
      return 1;
   }

   if (cmd == "subscribe_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res != "ok")
         throw std::runtime_error("leave_after_n_posts::subscribe_ack");

      return 1;
   }

   if (cmd == "post") {
      if (--op.n_posts == 0) {
         std::cout << "User " << op.user << " ok. (leave_after_n_posts)."
                   << std::endl;
         return -1;
      }

      return 1;
   }

   std::cout << j << std::endl;
   throw std::runtime_error("leave_after_n_posts::inexistent");
   return -1;
}

int leave_after_n_posts::on_handshake(std::shared_ptr<client_type> s)
{
   auto const str = make_login_cmd(op.user);
   s->send_msg(str);
   return 1;
}

int leave_after_n_posts::on_closed(boost::system::error_code ec)
{
   throw std::runtime_error("leave_after_n_posts::on_closed");
   return -1;
};

//______________

int simulator::on_read( std::string msg
                    , std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);

   auto const cmd = j.at("cmd").get<std::string>();
   if (cmd == "login_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res != "ok")
         throw std::runtime_error("simulator::login_ack");

      menus = j.at("menus").get<menu_elems_array_type>();

      json j_sub;
      j_sub["cmd"] = "subscribe";
      j_sub["last_post_id"] = 0;
      j_sub["channels"] = create_channels2(menus);
      j_sub["filter"] = 0;
      s->send_msg(j_sub.dump());
      return 1;
   }

   if (cmd == "subscribe_ack") {
      auto const res = j.at("result").get<std::string>();
      if (res != "ok")
         throw std::runtime_error("simulator::subscribe_ack");

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
      if (type == "server_ack")
         return 1; // Fix the server and remove this.

      if (type == "chat" || type == "chat_redirected") {
         // Used only by the app. Not in the tests.
         auto const post_id = j.at("post_id").get<long long>();
         auto const from = j.at("from").get<std::string>();

         ack_chat(from, post_id, s, "app_ack_received");
         ack_chat(from, post_id, s, "app_ack_read");
         send_chat_msg(from, post_id, s);
         return 1;
      }

      if (type == "app_ack_received" || type == "app_ack_read") {
         if (++counter == op.counter) {
            counter = 0;
            return 1;
         }

         std::cout << "Ack received. Sending new msg ..." << std::endl;
         auto const post_id = j.at("post_id").get<long long>();
         auto const from = j.at("from").get<std::string>();
         send_chat_msg(from, post_id, s);
         return 1;
      }

      if (type == "app_ack_read") {
         return 1;
      }
   }

   std::cout << j << std::endl;
   throw std::runtime_error("simulator::inexistent");
   return -1;
}

int simulator::on_handshake(std::shared_ptr<client_type> s)
{
   auto const str = make_login_cmd(op.user);
   s->send_msg(str);
   return 1;
}

int simulator::on_closed(boost::system::error_code ec)
{
   throw std::runtime_error("simulator::on_closed");
   return -1;
};

void
simulator::send_chat_msg( std::string to, long long post_id
                      , std::shared_ptr<client_type> s)
{
   auto const n = std::stoi(op.user.id);
   json j;
   j["cmd"] = "message";
   j["type"] = "chat";
   j["refers_to"] = -1;
   j["to"] = to;
   j["msg"] = "Message" + std::to_string(counter);
   j["post_id"] = post_id;
   j["is_sender_post"] = false;
   j["nick"] = nicks.at(n % std::size(nicks));

   s->send_msg(j.dump());
}

void
simulator::ack_chat( std::string to, long long post_id
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

      auto const j_menu = json::parse(op.menu);
      auto const menus = j_menu.at("menus").get<menu_elems_array_type>();
      auto const menu_codes = create_channels(menus);
      auto const pub_codes = channel_codes(menu_codes, menus);

      assert(!std::empty(pub_codes));

      auto pusher = [this](auto const& o)
         { pub_stack.push({-1, o}); };

      std::for_each(std::begin(pub_codes), std::end(pub_codes), pusher);

      json j_sub;
      j_sub["cmd"] = "subscribe";
      j_sub["last_post_id"] = 0;
      j_sub["channels"] = create_channels2(menus);
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

      pub_stack.top().first = j.at("id").get<int>();
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

      if (pub_stack.top().first != items.front().id)
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
         if (pub_stack.top().first != post_id2) {
            std::cout << op.user << " " << pub_stack.top().first << " != " << post_id2
                      << " " << msg << std::endl;
            throw std::runtime_error("publisher::on_read6");
         }

         //std::cout << op.user << " message " << post_id << " "
         //          << user_msg_counter << std::endl;
         --user_msg_counter;
         return handle_msg(s);
      }
   }

   if (cmd == "delete_ack") {
      pub_stack.pop();
      if (std::empty(pub_stack)) {
         std::cout << "User " << op.user << " ok. (Publisher)."
                   << std::endl;
         return -1;
      }

      server_echo = false;
      user_msg_counter = op.n_repliers;
      //std::cout << "=====> " << op.user << " " << post_id
      //          << " " << user_msg_counter <<  std::endl;
      return send_post(s);
   }

   std::cout << "Error: publisher ===> " << cmd << std::endl;
   throw std::runtime_error("publisher::on_read4");
   return -1;
}

int publisher::handle_msg(std::shared_ptr<client_type> s)
{
   if (server_echo && pub_stack.top().first != -1 && user_msg_counter == 0) {
      // We are done with this post and can delete it from the server.
      json j_sub;
      j_sub["cmd"] = "delete";
      j_sub["id"] = pub_stack.top().first;
      j_sub["to"] = pub_stack.top().second;
      s->send_msg(j_sub.dump());
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
   auto const str = make_post_cmd(pub_stack.top().second);
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

      auto const menus = j.at("menus").get<menu_elems_array_type>();
      auto const menu_codes = create_channels(menus);
      auto const pub_codes = channel_codes(menu_codes, menus);

      assert(!std::empty(pub_codes));

      auto f = [s, this](auto const& o)
         { pub(o, s); };

      // Sending the publishes without waiting for the acks is not the
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
   json sub;
   sub["msg"] = "Not an interesting message.";
   sub["nick"] = "Wheeler";
   sub["avatar"] = "";

   post item {-1, op.user.id, sub.dump(), pub_code, 0, 0};

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
   auto const code = menu_code_type {{{{0, 0, 0, 0}}, {{0, 0, 0, 0}}}};
   auto const str = make_post_cmd(code);
   s->send_msg(str, -1);
}

//________________________________

std::vector<login>
test_reg(session_shell_cfg const& cfg, launcher_cfg const& lcfg)
{
   boost::asio::io_context ioc;

   using client_type = register1;
   using config_type = client_type::options_type;

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

   std::vector<login> logins;
   std::transform( std::cbegin(sessions), std::cend(sessions)
                 , std::back_inserter(logins), f);

   return logins;
}

}

