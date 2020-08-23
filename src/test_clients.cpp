#include "test_clients.hpp"

#include <chrono>

#include "client_session.hpp"
#include "session_launcher.hpp"

namespace occase::cli
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

std::string make_login_cmd(occase::cli::login const& cred)
{
   json j;
   j["cmd"] = "login";
   j["user"] = cred.user;
   j["key"] = cred.key;
   return j.dump();
}

std::string make_post_cmd()
{
   occase::post p
   { std::chrono::seconds {0} // date
   , {}  // id
   , {}  // on_search
   , {}  // views
   , {}  // detailed_views
   , {} // from
   , "Wheeler" // nick
   , "9ac8316ca55e6d888d43092fd73a78d6" // avatar
   , "Some description." // description
   , {} // location
   , {} // product
   , {} // ex
   , {} // in
   , {} // range_values
   , {} // images
   };

   json j;
   j["cmd"] = "publish";
   j["post"] = p;
   return j.dump();
}

void check_result(json const& j, char const* expected, char const* error)
{
   auto const res = j.at("result").get<std::string>();
   if (res != expected)
      throw std::runtime_error(error);
}

std::string make_sub_payload()
{
   json j_sub;
   j_sub["cmd"] = "subscribe";

   return j_sub.dump();
}

int replier::on_read(std::string msg, std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);

   auto const cmd = j.at("cmd").get<std::string>();
   if (cmd == "login_ack") {
      check_result(j, "ok", "replier::login_ack");

      to_receive_posts = op.n_publishers;

      std::cout << "Sub: User " << op.cred
                << " expects: " << to_receive_posts
                << std::endl;

      s->send_msg(make_sub_payload());
      return 1;
   }

   if (cmd == "subscribe_ack") {
      check_result(j, "ok", "replier::subscribe_ack");
      return 1;
   }

   if (cmd == "post") {
      auto posts = j.at("posts").get<std::vector<post>>();

      auto const f = [this, s](auto const& e)
         { send_chat_msg(e.from, e.id, s); };

      std::for_each(std::begin(posts), std::end(posts), f);

      return 1;
   }

   if (cmd == "message") {
      auto const type = j.at("type").get<std::string>();
      if (type == "server_ack") {
         //std::cout << "Codition not met: " << (to_receive_posts - 1)
         //          << std::endl;
         if (--to_receive_posts == 0) {
            std::cout << "User " << op.cred << " done. (Replier)."
                      << std::endl;
            return -1; // Done.
         }

         return 1; // Fix the server and remove this.
      }

      if (type == "chat") {
         std::cout << "Should not get here in the tests." << std::endl;
         return 1;
      }

      // Currently we make no use of these commands.
      if (type == "chat_ack_received") {
         return 1;
      }

      if (type == "chat_ack_read") {
         return 1;
      }
   }

   if (cmd == "presence")
      return 1;

   std::cout << j << std::endl;
   throw std::runtime_error("replier::inexistent");
   return -1;
}

int replier::on_handshake(std::shared_ptr<client_type> s)
{
   auto const str = make_login_cmd(op.cred);
   s->send_msg(str);
   return 1;
}

int replier::on_closed(boost::system::error_code ec)
{
   throw std::runtime_error("replier::on_closed");
   return -1;
};

void
replier::send_chat_msg( std::string to, std::string const& post_id
                      , std::shared_ptr<client_type> s)
{
   //std::cout << "Sub: User " << op.cred << " sending to " << to
   //          << ", post_id: " << post_id << std::endl;

   json j;
   j["cmd"] = "message";
   j["is_redirected"] = 0;
   j["type"] = "chat";
   j["refers_to"] = -1;
   j["to"] = to;
   j["message"] = "Tenho interesse nesse carro, podemos conversar?";
   j["post_id"] = post_id;
   j["nick"] = nicks.at(std::size(nicks) - 1);
   j["id"] = 23;

   s->send_msg(j.dump());
}

//_______________________________________

int
leave_after_sub_ack::on_read( std::string msg
                            , std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);

   auto const cmd = j.at("cmd").get<std::string>();
   if (cmd == "login_ack") {
      check_result(j, "ok", "leave_after_sub_ack::login_ack");
      s->send_msg(make_sub_payload());
      return 1;
   }

   if (cmd == "subscribe_ack") {
      check_result(j, "ok", "leave_after_sub_ack::subscribe_ack");
      std::cout << "User " << op.cred << ": ok" << std::endl;
      return -1;
   }

   if (cmd == "presence")
      return 1;

   std::cout << j << std::endl;
   throw std::runtime_error("leave_after_sub_ack::inexistent");
   return -1;
}

int leave_after_sub_ack::on_handshake(std::shared_ptr<client_type> s)
{
   auto const str = make_login_cmd(op.cred);
   s->send_msg(str);
   return 1;
}

int leave_after_sub_ack::on_closed(boost::system::error_code ec)
{
   throw std::runtime_error("leave_after_sub_ack::on_closed");
   return -1;
};

//_______________________________________

int
leave_after_n_posts::on_read( std::string msg
                            , std::shared_ptr<client_type> s)
{
   if (start_counting)
      return check_counter();

   auto const j = json::parse(msg);

   auto const cmd = j.at("cmd").get<std::string>();
   if (cmd == "login_ack") {
      check_result(j, "ok", "leave_after_n_posts::login_ack");

      s->send_msg(make_sub_payload());
      return 1;
   }

   if (cmd == "subscribe_ack") {
      check_result(j, "ok", "leave_after_n_posts::subscribe_ack");
      return 1;
   }

   if (cmd == "post") {
      start_counting = true;
      return check_counter();
   }

   if (cmd == "presence")
      return 1;

   std::cout << j << std::endl;
   throw std::runtime_error("leave_after_n_posts::inexistent");
   return -1;
}

int leave_after_n_posts::check_counter()
{
   if (--op.n_posts == 0) {
      std::cout << "User " << op.cred << " ok. (leave_after_n_posts)."
                << std::endl;
      return -1;
   }

   return 1;
}

int leave_after_n_posts::on_handshake(std::shared_ptr<client_type> s)
{
   auto const str = make_login_cmd(op.cred);
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
      check_result(j, "ok", "simulator::login_ack");
      s->send_msg(make_sub_payload());
      return 1;
   }

   if (cmd == "subscribe_ack") {
      check_result(j, "ok", "simulator::subscribe_ack");
      return 1;
   }

   if (cmd == "post") {
      auto posts = j.at("posts").get<std::vector<post>>();

      if (op.counter == 0)
         return 1;

      auto const f = [this, s](auto const& e)
         { send_chat_msg(e.from, e.id, s); };

      std::for_each(std::begin(posts), std::end(posts), f);

      return 1;
   }

   if (cmd == "message") {
      auto const type = j.at("type").get<std::string>();
      if (type == "server_ack")
         return 1; // Fix the server and remove this.

      if (type == "chat") {
         // Used only by the app. Not in the tests.
         auto const post_id = j.at("post_id").get<std::string>();
         auto const from = j.at("from").get<std::string>();
         auto const id = j.at("id").get<int>();

         ack_chat(from, post_id, id, s, "chat_ack_received");
         ack_chat(from, post_id, id, s, "chat_ack_read");
         send_chat_msg(from, post_id, s);
         return 1;
      }

      if (type == "chat_ack_received" || type == "chat_ack_read") {
         if (++counter == op.counter) {
            counter = 0;
            return 1;
         }

         std::cout << "Ack received. Sending new msg ..." << std::endl;
         auto const post_id = j.at("post_id").get<std::string>();
         auto const from = j.at("from").get<std::string>();
         send_chat_msg(from, post_id, s);
         return 1;
      }

      if (type == "chat_ack_read") {
         return 1;
      }
   }

   if (cmd == "presence")
      return 1;

   std::cout << j << std::endl;
   throw std::runtime_error("simulator::inexistent");
   return -1;
}

int simulator::on_handshake(std::shared_ptr<client_type> s)
{
   auto const str = make_login_cmd(op.cred);
   s->send_msg(str);
   return 1;
}

int simulator::on_closed(boost::system::error_code ec)
{
   throw std::runtime_error("simulator::on_closed");
   return -1;
};

void simulator::
send_chat_msg( std::string to
             , std::string const& post_id
             , std::shared_ptr<client_type> s)
{
   json j;
   j["cmd"] = "message";
   j["is_redirected"] = 0;
   j["type"] = "chat";
   j["refers_to"] = -1;
   j["to"] = to;
   j["message"] = "Message " + std::to_string(counter);
   j["post_id"] = post_id;
   j["nick"] = nicks.back();
   j["id"] = 22;

   s->send_msg(j.dump());
}

void simulator::
ack_chat( std::string const& to
        , std::string const& post_id
        , int id
        , std::shared_ptr<client_type> s
        , std::string const& type)
{
   json j;
   j["cmd"] = "message";
   j["type"] = type;
   j["to"] = to;
   j["post_id"] = post_id;
   j["nick"] = "Zebu";
   j["ack_ids"] = std::vector<int>{id};
   j["id"] = -1;

   s->send_msg(j.dump());
}

//_______________

int publisher::on_read(std::string msg, std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);

   auto const cmd = j.at("cmd").get<std::string>();
   if (cmd == "login_ack") {
      check_result(j, "ok", "publisher::login_ack");
      pub_stack.push({}); 
      s->send_msg(make_sub_payload());
      return 1;
   }

   if (cmd == "subscribe_ack") {
      check_result(j, "ok", "publisher::subscribe_ack");
      return send_post(s);
   }

   if (cmd == "publish_ack") {
      check_result(j, "ok", "publisher::publish_ack");
      pub_stack.top() = j.at("id").get<std::string>();
      //std::cout << op.cred << " publish_ack " << post_id << std::endl;
      return handle_msg(s);
   }

   if (cmd == "post") {
      // We are only interested in our own publishes at the moment.
      auto posts = j.at("posts").get<std::vector<post>>();

      auto cond = [this](auto const& e)
         { return e.from != op.cred.user_id; };

      posts.erase( std::remove_if( std::begin(posts), std::end(posts)
                                 , cond)
                 , std::end(posts));

      // Ignores messages that are not our own.
      if (std::empty(posts))
         return 1;

      if (std::size(posts) != 1) {
         std::cout << std::size(posts) << " " << msg << std::endl;
         throw std::runtime_error("publisher::publish1");
      }

      // Since we send only one publish at time posts should contain
      // only one item now.

      if (pub_stack.top() != posts.front().id)
         throw std::runtime_error("publisher::publish2");

      //std::cout << op.cred << " publish echo " << post_id << std::endl;
      server_echo = true;
      return handle_msg(s);
   }

   if (cmd == "message") {
      auto const type = j.at("type").get<std::string>();
      if (type == "server_ack")
         return 1;

      if (type == "chat") {
         // This test is to make sure the server is not sending us a
         // message meant to some other cred.
         auto const to = j.at("to").get<std::string>();
         if (to != op.cred.user_id)
            throw std::runtime_error("publisher::on_read5");

         auto const post_id2 = j.at("post_id").get<std::string>();
         if (pub_stack.top() != post_id2) {
            std::cout << op.cred << " " << pub_stack.top() << " != " << post_id2
                      << " " << msg << std::endl;
            throw std::runtime_error("publisher::on_read6");
         }

         //std::cout << op.cred << " message " << post_id << " "
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
   if (server_echo && !std::empty(pub_stack.top()) && user_msg_counter == 0) {
      pub_stack.pop();
      if (std::empty(pub_stack)) {
         std::cout << "User " << op.cred << " ok. (Publisher)."
                   << std::endl;
         return -1;
      }

      server_echo = false;
      user_msg_counter = op.n_repliers;
      //std::cout << "=====> " << op.cred << " " << post_id
      //          << " " << user_msg_counter <<  std::endl;
      return send_post(s);
   }

   return 1;
}

int publisher::on_handshake(std::shared_ptr<client_type> s)
{
   auto const str = make_login_cmd(op.cred);
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
   s->send_msg(make_post_cmd());
   return 1;
}

//________________________________

int publisher2::on_read(std::string msg, std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);

   auto const cmd = j.at("cmd").get<std::string>();
   if (cmd == "login_ack") {
      check_result(j, "ok", "publisher2::login_ack");
      pub(s);
      msg_counter = 1;
      return 1;
   }

   if (cmd == "publish_ack") {
      check_result(j, "ok", "publisher2::publish_ack");

      auto const post_id = j.at("id").get<std::string>();
      post_ids.push_back(post_id);
      if (--msg_counter == 0) {
         std::cout << "User " << op.cred << " ok. (Publisher2)."
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
   auto const str = make_login_cmd(op.cred);
   s->send_msg(str);
   return 1;
}

int publisher2::on_closed(boost::system::error_code ec)
{
   throw std::runtime_error("publisher2::on_closed");
   return -1;
};

int publisher2::pub(std::shared_ptr<client_type> s) const
{
   occase::post item
   { std::chrono::seconds {0} // date
   , {}  // id
   , {}  // on_search
   , {}  // views
   , {}  // detailed_views
   , op.cred.user_id // from
   , "Wheeler" // nick
   , "" // avatar
   , "Some description." // description
   , {} // location
   , {} // product
   , {} // ex
   , {} // in
   , {} // range_values
   , {} // images
   };

   json j_msg;
   j_msg["cmd"] = "publish";
   j_msg["post"] = item;
   s->send_msg(j_msg.dump());
   return 1;
}

//________________________________

int msg_pull::on_read(std::string msg, std::shared_ptr<client_type> s)
{
   std::cout << msg << std::endl;
   auto const j = json::parse(msg);

   auto const cmd = j.at("cmd").get<std::string>();
   if (cmd == "login_ack") {
      check_result(j, "ok", "msg_pull::login_ack");
      return 1;
   }

   if (cmd == "message") {
      auto const type = j.at("type").get<std::string>();
      if (type == "server_ack") {
         return 1;
      }

      if (type == "chat") {
         auto const post_id = j.at("post_id").get<std::string>();
         post_ids.push_back(post_id);
         //std::cout << "Expecting: " << op.expected_user_msgs
         //          << std::endl;
         if (--op.expected_user_msgs == 0) {
            std::cout << "User " << op.cred << " ok. (msg_pull)."
                      << std::endl;
            return -1;
         }
         return 1;
      }
   }

   if (cmd == "publish_ack") {
      check_result(j, "ok", "msg_pull::publish_ack");

      //auto const post_id = j.at("id").get<int>();
      //std::cout << op.cred << " publish_ack " << post_id << std::endl;
      return -1;
   }

   throw std::runtime_error("msg_pull::on_read4");
   return -1;
}

int msg_pull::on_handshake(std::shared_ptr<client_type> s)
{
   auto const str = make_login_cmd(op.cred);
   s->send_msg(str);
   return 1;
}

//________________________________

int register1::on_read(std::string msg, std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);

   auto const cmd = j.at("cmd").get<std::string>();
   if (cmd == "register_ack") {
      check_result(j, "ok", "register1::login_ack");

      op.cred.user = j.at("user").get<std::string>();
      op.cred.key = j.at("key").get<std::string>();
      op.cred.user_id = j.at("user_id").get<std::string>();
      std::cout << "User " << op.cred << " ok. (register1)."
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
      check_result(j, "fail", "login_err::on_read1");
      return 1;
   }

   std::cout << "User " << op.cred << " ok. (login_err)."
             << std::endl;
   throw std::runtime_error("login_err::on_read");
   return -1;
}

int login_err::on_handshake(std::shared_ptr<client_type> s)
{
   auto str = make_login_cmd(op.cred);
   s->send_msg(str);
   return 1;
}

//________________________________

int early_close::on_read(std::string msg, std::shared_ptr<client_type> s)
{
   auto const j = json::parse(msg);

   auto const cmd = j.at("cmd").get<std::string>();
   if (cmd == "login_ack") {
      check_result(j, "ok", "early_close::login_ack");
      send_post(s);
      return 1;
   }

   std::cout << "early_close: " << msg << std::endl;

   throw std::runtime_error("early_close::on_read4");
   return -1;
}

int early_close::on_handshake(std::shared_ptr<client_type> s)
{
   auto const str = make_login_cmd(op.cred);
   s->send_msg(str);
   return 1;
}

void early_close::send_post(std::shared_ptr<client_type> s) const
{
   auto const str = make_post_cmd();
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

