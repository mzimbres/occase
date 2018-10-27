#include "server_mgr.hpp"

#include "server_session.hpp"
#include "resp.hpp"

ev_res on_message( server_mgr& mgr
                 , std::shared_ptr<server_session> s
                 , std::string msg)
{
   auto const j = json::parse(msg);
   //std::cout << j << std::endl;

   auto const cmd = j.at("cmd").get<std::string>();

   if (s->is_waiting_auth()) {
      if (cmd == "login")
         return mgr.on_login(std::move(j), s);

      if (cmd == "auth")
         return mgr.on_auth(std::move(j), s);

      std::cerr << "Server: Unknown command " << cmd << std::endl;
      return ev_res::unknown;
   }

   if (s->is_waiting_sms()) {
      if (cmd == "sms_confirmation")
         return mgr.on_sms_confirmation(std::move(j), s);

      std::cerr << "Server: Unknown command " << cmd << std::endl;
      return ev_res::unknown;
   }

   if (s->is_auth()) {
      if (cmd == "create_group")
         return mgr.on_create_group(std::move(j), s);

      if (cmd == "join_group")
         return mgr.on_join_group(std::move(j), s);

      if (cmd == "group_msg")
         return mgr.on_user_channel_msg(std::move(msg), std::move(j), s);

      if (cmd == "user_msg")
         return mgr.on_user_msg(std::move(j), s);

      std::cerr << "Server: Unknown command " << cmd << std::endl;
      return ev_res::unknown;
   }

   std::cerr << "Server: Unknown command " << cmd << std::endl;
   return ev_res::unknown;
}

server_mgr::server_mgr(server_mgr_cf cf, asio::io_context& ioc)
: timeouts(cf.get_timeouts())
, redis_sub_session(cf.get_redis_session_cf(), ioc)
, redis_pub_session(cf.get_redis_session_cf(), ioc)
{
   redis_sub_session.run();

   // TODO: Make exception safe.
   auto const handler1 = [this](auto ec, auto&& data)
   {
      if (ec) {
         std::cout << "sub_handler: " << ec.message() << std::endl;
         return;
      }

      if (data.front() == "message") {
         assert(std::size(data) == 3);
         assert(data[1] == "channels_msgs");
         on_group_msg(std::move(data.back()));
         return;
      }

      //for (auto const& o : data)
      //   std::cout << o << " ";
      //std::cout << std::endl;
   };

   redis_sub_session.set_msg_handler(handler1);

   using namespace aedis;

   // TODO: In the action handler we have to ignore subscription messages
   // That happen to arrive while we are sending e.g. a subscription
   // to another channel.
   redis_sub_session.send(gen_resp_cmd("SUBSCRIBE", {"channels_msgs"}));

   redis_pub_session.run();

   auto const handler2 = [](auto ec, auto data)
   {
      if (ec) {
         std::cout << "pub_handler: " << ec.message() << std::endl;
         return;
      }

      //for (auto const& o : data)
      //   std::cout << o << " ";
      //std::cout << std::endl;
   };

   redis_pub_session.set_msg_handler(handler2);
}

ev_res server_mgr::on_login(json j, std::shared_ptr<server_session> s)
{
   auto const tel = j.at("tel").get<std::string>();

   // TODO: Replace this with a query to the database.
   if (sessions.find(tel) != std::end(sessions)) {
      // The user already exists in the system.
      json resp;
      resp["cmd"] = "login_ack";
      resp["result"] = "fail";
      s->send(resp.dump());
      return ev_res::login_fail;
   }

   s->set_id(tel);

   // TODO: Use a random number generator with six digits.
   s->set_sms("8347");

   json resp;
   resp["cmd"] = "login_ack";
   resp["result"] = "ok";
   s->send(resp.dump());
   return ev_res::login_ok;
}

ev_res server_mgr::on_auth(json j, std::shared_ptr<server_session> s)
{
   auto const from = j.at("from").get<std::string>();

   auto const new_user = sessions.insert({from, s});
   if (!new_user.second) {
      // The user is already logged into the system. We do not allow
      // this yet.
      json resp;
      resp["cmd"] = "auth_ack";
      resp["result"] = "fail";
      s->send(resp.dump());
      return ev_res::auth_fail;
   }

   // TODO: Query the database to validate the session.
   //if (from != s->get_id()) {
   //   // Incorrect id.
   //   json resp;
   //   resp["cmd"] = "auth_ack";
   //   resp["result"] = "fail";
   //   s->send(resp.dump());
   //   return ev_res::auth_fail;
   //}

   s->set_id(from);
   s->promote();
   //new_user.first.second->set_session(s);

   json resp;
   resp["cmd"] = "auth_ack";
   resp["result"] = "ok";
   s->send(resp.dump());
   return ev_res::auth_ok;
}

ev_res
server_mgr::on_sms_confirmation(json j, std::shared_ptr<server_session> s)
{
   auto const tel = j.at("tel").get<std::string>();
   auto const sms = j.at("sms").get<std::string>();

   if (sms != s->get_sms()) {
      json resp;
      resp["cmd"] = "sms_confirmation_ack";
      resp["result"] = "fail";
      s->send(resp.dump());
      return ev_res::sms_confirmation_fail;
   }

   s->promote();

   // Inserts the user in the system.
   auto const id = s->get_id();
   assert(!std::empty(id));
   auto const new_user = sessions.insert({tel, s});

   // This would be odd. The entry already exists on the index map
   // which means we did something wrong in the login command.
   assert(new_user.second);

   json resp;
   resp["cmd"] = "sms_confirmation_ack";
   resp["result"] = "ok";
   s->send(resp.dump());
   return ev_res::sms_confirmation_ok;
}

ev_res
server_mgr::on_create_group(json j, std::shared_ptr<server_session> s)
{
   auto const hash = j.at("hash").get<std::string>();

   auto const new_group = channels.insert({hash, {}});
   if (!new_group.second) {
      // Group already exists.
      json resp;
      resp["cmd"] = "create_group_ack";
      resp["result"] = "fail";
      resp["code"] = hash;
      auto const tmp = resp.dump();
      s->send(tmp);
      //std::cout << tmp << std::endl;
      return ev_res::create_group_fail;
   }

   json resp;
   resp["cmd"] = "create_group_ack";
   resp["result"] = "ok";
   resp["code"] = hash;
   auto const tmp = resp.dump();
   //std::cout << tmp << std::endl;

   s->send(std::move(tmp));
   return ev_res::create_group_ok;
}

ev_res
server_mgr::on_join_group(json j, std::shared_ptr<server_session> s)
{
   auto const hash = j.at("hash").get<std::string>();

   auto const g = channels.find(hash);
   if (g == std::end(channels)) {
      json resp;
      resp["cmd"] = "join_group_ack";
      resp["result"] = "fail";
      s->send(resp.dump());
      return ev_res::join_group_fail;
   }

   auto const from = s->get_id();
   g->second.insert({from, s});

   json resp;
   resp["cmd"] = "join_group_ack";
   resp["result"] = "ok";

   s->send(resp.dump());
   return ev_res::join_group_ok;
}

void broadcast_msg(channel_type& members, std::string msg)
{
   auto begin = std::begin(members);
   auto end = std::end(members);
   while (begin != end) {
      if (auto s = begin->second.lock()) {
         // The user is online. We can just forward his message.
         // TODO: We incurr here the possibility of sending repeated
         // messages to the user. The scenario is as follows
         // 1. We send a message bellow and it is buffered.
         // 2. The user disconnects before receiving the message.
         // 3. The message are saved on the database.
         // 4. The user reconnects and we read and send him his
         //    messages from the database.
         // 5. We traverse the channels sending him the latest messages
         //    the he missed while he was offline and this message is
         //    between them.
         // This is perhaps unlikely but should be avoided in the
         // future.
         s->send(msg);
         ++begin;
         continue;
      }

      // Removes users that are not online anymore.
      begin = members.erase(begin);
   }
}

ev_res
server_mgr::on_user_channel_msg( std::string msg, json j
                               , std::shared_ptr<server_session> s)
{
   auto const to = j.at("to").get<std::string>();

   // Looks like this should be removed.
   //auto const g = channels.find(to);
   //if (g == std::end(channels)) {
   //   // This is a non-existing channel. Perhaps the json command was
   //   // sent with the wrong information signaling a logic error in
   //   // the app.

   //   json resp;
   //   resp["cmd"] = "group_msg_ack";
   //   resp["result"] = "fail";
   //   s->send(resp.dump());
   //   return ev_res::group_msg_fail;
   //}

   auto rcmd = aedis::gen_resp_cmd( "PUBLISH"
                                  , {"channels_msgs", std::move(msg)});
   redis_pub_session.send(std::move(rcmd));

   // TODO: This ack should (maybe) be moved to the function that
   // receives the broadcasted message from redis subscription group.
   // But this would cause the client to try to repeat the operation
   // if for example redis is not available at the moment and that
   // would cause duplicated messages.
   json ack;
   ack["cmd"] = "group_msg_ack";
   ack["result"] = "ok";
   s->send(ack.dump());
   return ev_res::group_msg_ok;
}

void server_mgr::on_group_msg(std::string msg)
{
   auto const j = json::parse(msg);

   auto const to = j.at("to").get<std::string>();

   auto const g = channels.find(to);
   if (g == std::end(channels)) {
      // This is a non-existing group. Perhaps the json command was
      // sent with the wrong information signaling a logic error in
      // the app.
      return;
   }

   // TODO: Change broadcast to return the number of users that the
   // message has reached. 
   //std::cout << "on_group_msg: sending " << msg << std::endl;
   broadcast_msg(g->second, std::move(msg));
}

void server_mgr::release_user(std::string id)
{
   sessions.erase(id);
}

ev_res
server_mgr::on_user_msg(json j, std::shared_ptr<server_session> s)
{
   //auto const from = j["from"].get<user_bind>();
   //auto const to = j["to"].get<user_bind>();

   //users.at(to.index).send(j.dump());
   return ev_res::user_msg_ok;
}

void server_mgr::shutdown()
{
   std::cout << "Shutting down " << std::size(sessions) 
             << " user sessions ..." << std::endl;

   for (auto o : sessions)
      if (auto s = o.second.lock())
         s->shutdown();

   std::cout << "Shuting down redis subscribe session ..." << std::endl;
   redis_sub_session.close();
   std::cout << "Shuting down redis publish session ..." << std::endl;
   redis_pub_session.close();
}

