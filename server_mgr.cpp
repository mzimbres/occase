#include "server_mgr.hpp"

#include "resp.hpp"
#include "menu_parser.hpp"
#include "server_session.hpp"

namespace rt
{

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
      if (cmd == "join_group")
         return mgr.on_join_group(std::move(j), s);

      if (cmd == "group_msg")
         return mgr.on_user_group_msg(std::move(msg), std::move(j), s);

      if (cmd == "user_msg")
         return mgr.on_user_msg(std::move(msg), std::move(j), s);

      std::cerr << "Server: Unknown command " << cmd << std::endl;
      return ev_res::unknown;
   }

   std::cerr << "Server: Unknown command " << cmd << std::endl;
   return ev_res::unknown;
}

void broadcast_msg(channel_type& members, std::string const& msg)
{
   //std::cout << "Broadcast size: " << std::size(members) << std::endl;
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
         //    that he missed while he was offline and this message is
         //    between them.
         // This is perhaps unlikely but should be avoided in the
         // future.
         //std::cout << "on_group_msg: sending to " << s->get_id()
         //          << " " << msg << std::endl;
         s->send(msg);
         ++begin;
         continue;
      }

      // Removes users that are not online anymore.
      begin = members.erase(begin);
   }
}

server_mgr::server_mgr(server_mgr_cf cf, asio::io_context& ioc_)
: ioc(ioc_)
, timeouts(cf.get_timeouts())
, redis_gsub_session(cf.get_redis_session_cf(), ioc)
, redis_ksub_session(cf.get_redis_session_cf(), ioc)
, redis_pub_session(cf.get_redis_session_cf(), ioc)
, redis_group_channel(cf.redis_group_channel)
, redis_menu_key(cf.redis_menu_key)
, redis_msg_prefix(cf.redis_msg_prefix + ":")
, redis_notify_prefix(redis_keyspace_prefix + redis_msg_prefix)
, stats_timer(ioc)
{
   auto const handler = [this]( auto const& ec , auto const& data
                               , auto const& req)
   { redis_pub_msg_handler(ec, data, req); };

   redis_pub_session.set_on_msg_handler(handler);
   redis_pub_session.run();

   // Asynchronously retrieves the menu.
   redis_pub_session.send(gen_resp_cmd( redis_cmd::get
                                      , {redis_menu_key}
                                      , redis_menu_key));
   do_stats_logger();
}

void
server_mgr::redis_group_msg_handler( boost::system::error_code const& ec
                                   , std::vector<std::string> const& data
                                   , redis_req const& req)
{
   if (ec) {
      std::cout << "sub_handler: " << ec.message() << std::endl;
      return;
   }

   if (req.cmd == redis_cmd::unsolicited) {
      assert(data.front() == "message");
      assert(std::size(data) == 3);
      assert(data[1] == redis_group_channel);

      auto const tmp = [this, msg = std::move(data.back())]()
      {
         auto const j = json::parse(msg);
         auto const to = j.at("to").get<std::string>();
         auto const g = channels.find(to);
         if (g == std::end(channels)) {
            // Should not happen as the group is checked on
            // on_user_group:msg before being sent to redis for broadcast.
            assert(false);
            return;
         }

         broadcast_msg(g->second, msg);
      };

      asio::post(ioc, tmp);

      return;
   }
}

void 
server_mgr::redis_key_msg_handler( boost::system::error_code const& ec
                                 , std::vector<std::string> const& data
                                 , redis_req const& req)
{
   if (ec) {
      std::cout << "sub_handler: " << ec.message() << std::endl;
      return;
   }

   if (req.cmd == redis_cmd::unsolicited) {
      if (data.back() == "rpush") {
         assert(data.front() == "message");
         assert(std::size(data) == 3);
         auto const n = data[1].rfind(":");
         assert(n != std::string::npos);

         // We have to retrieve the user message.
         auto const user_id = data[1].substr(n + 1);
         auto const key = redis_msg_prefix + user_id;
         auto const rcmd = gen_resp_cmd(redis_cmd::lpop, {key}, user_id);

         //std::cout << "sending to " << user_id << std::endl;
         redis_pub_session.send(std::move(rcmd));

         //auto const s = sessions.find(user_id);
         //if (s == std::end(sessions)) {
         //   // Should not happen as we unsubscribe from the user message
         //   // channel we the user goes offline.
         //   assert(false);
         //   return;
         //}
      }
   }
}

void
server_mgr::redis_pub_msg_handler( boost::system::error_code const& ec
                                 , std::vector<std::string> const& data
                                 , redis_req const& req)
{
   if (ec) {
      std::cout << "pub_handler: " << ec.message() << std::endl;
      return;
   }

   if (req.cmd == redis_cmd::lpop) {
      assert(std::size(data) == 1);
      //std::cout << req.user_id << " ===> " << data.back() << std::endl;
      auto const match = sessions.find(req.user_id);
      if (match == std::end(sessions)) {
         // TODO: The user went offline. We have to enqueue the
         // message again. Rethink this.
         return;
      }

      if (auto s = match->second.lock()) {
         s->send(data.back());
      } else {
         // The user went offline but the session was not removed from
         // the map. This is perhaps not a bug but undesirable as we
         // do not have garbage collector for expired sessions.
         assert(false);
      }
   }

   if (req.cmd == redis_cmd::get) {
      assert(std::size(data) == 1);
      menu = data.back();
      auto const j_menu = json::parse(data.back());
      auto const codes = get_hashes(std::move(j_menu));
      if (std::empty(codes)) { // TODO: Report error here.
         std::cerr << "Group codes array empty." << std::endl;
      }

      for (auto const& gc : codes) {
         auto const new_group = channels.insert({gc, {}});
         if (new_group.second) {
            std::cout << "Successfully created channel: " << gc << std::endl;
         } else {
            std::cout << "Channel " << gc << " already exists." << std::endl;
         }
      }

      // After creating the groups we can establish other redis
      // connections.
      auto const handler1 = [this]( auto const& ec , auto const& data
                                  , auto const& req)
      { redis_group_msg_handler(ec, data, req); };

      redis_gsub_session.set_on_msg_handler(handler1);
      redis_gsub_session.run();
      redis_gsub_session.send(gen_resp_cmd( redis_cmd::subscribe
                                          , {redis_group_channel}));

      auto const handler3 = [this]( auto const& ec , auto const& data
                                  , auto const& req)
      { redis_key_msg_handler(ec, data, req); };

      redis_ksub_session.set_on_msg_handler(handler3);
      redis_ksub_session.run();
   }
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
   resp["menu"] = menu;
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
   assert(s->is_auth());

   auto const scmd = gen_resp_cmd( redis_cmd::subscribe
                                 , { redis_notify_prefix + s->get_id() });

   redis_ksub_session.send(std::move(scmd));

   json resp;
   resp["cmd"] = "auth_ack";
   resp["result"] = "ok";

   auto const menu_version = j.at("menu_version").get<int>();
   if (menu_version == -1)
      resp["menu"] = menu;

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
   assert(s->is_auth());

   // This would be odd. The entry already exists on the index map
   // which means we did something wrong in the login command.
   assert(new_user.second);

   auto const scmd = gen_resp_cmd( redis_cmd::subscribe
                                 , { redis_notify_prefix + s->get_id() });

   redis_ksub_session.send(std::move(scmd));

   json resp;
   resp["cmd"] = "sms_confirmation_ack";
   resp["result"] = "ok";
   s->send(resp.dump());
   return ev_res::sms_confirmation_ok;
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
   g->second[from] = s; // Overwrites any previous session.

   json resp;
   resp["cmd"] = "join_group_ack";
   resp["result"] = "ok";

   s->send(resp.dump());
   return ev_res::join_group_ok;
}

ev_res
server_mgr::on_user_group_msg( std::string msg, json j
                             , std::shared_ptr<server_session> s)
{
   auto const to = j.at("to").get<std::string>();

   // Looks like this should be removed.
   auto const g = channels.find(to);
   if (g == std::end(channels)) {
      // This is a non-existing channel. Perhaps the json command was
      // sent with the wrong information signaling a logic error in
      // the app.
      json resp;
      resp["cmd"] = "group_msg_ack";
      resp["result"] = "fail";
      resp["id"] = j.at("id").get<int>();
      s->send(resp.dump());
      return ev_res::group_msg_fail;
   }

   auto rcmd = gen_resp_cmd(redis_cmd::publish, { redis_group_channel, msg});

   redis_pub_session.send(std::move(rcmd));

   json ack;
   ack["cmd"] = "group_msg_ack";
   ack["result"] = "ok";
   ack["id"] = j.at("id").get<int>();
   s->send(ack.dump());
   return ev_res::group_msg_ok;
}

void server_mgr::release_auth_session(std::string const& id)
{
   auto const match = sessions.find(id);
   if (match == std::end(sessions)) {
      // This is a bug, all autheticated sessions should be in the
      // sessions map.
      assert(false);
      return;
   }

   sessions.erase(match); // We do not need the return value.

   auto const scmd = gen_resp_cmd( redis_cmd::unsubscribe
                                 , { redis_notify_prefix + id});

   redis_ksub_session.send(std::move(scmd));
}

ev_res
server_mgr::on_user_msg( std::string msg, json j
                       , std::shared_ptr<server_session> s)
{
   // TODO: Search the sessions map if the user is online and in this
   // node and send him his message directly to avoid overloading the
   // redis server. This would be a big optimization in the case of
   // small number of nodes.
   auto const scmd = gen_resp_cmd( redis_cmd::rpush
                                 , { redis_msg_prefix + s->get_id(), msg});

   redis_pub_session.send(std::move(scmd));

   json ack;
   ack["cmd"] = "user_msg_server_ack";
   ack["result"] = "ok";
   ack["id"] = j.at("id").get<int>();
   s->send(ack.dump());
   //std::cout << ack << std::endl;
   return ev_res::user_msg_ok;
}

void server_mgr::shutdown()
{
   std::cout << "Shutting down " << std::size(sessions) 
             << " user sessions ..." << std::endl;

   for (auto o : sessions)
      if (auto s = o.second.lock())
         s->shutdown();

   std::cout << "Shuting down redis group subscribe session ..."
             << std::endl;

   redis_gsub_session.close();

   std::cout << "Shuting down redis publish session ..."
             << std::endl;

   redis_pub_session.close();

   std::cout << "Shuting down redis user msg subscribe session ..."
             << std::endl;

   redis_ksub_session.close();

   stats_timer.cancel();
}

void server_mgr::do_stats_logger()
{
   stats_timer.expires_after(std::chrono::seconds{1});

   auto handler = [this](auto ec)
   {
      if (ec) {
         std::cout << "stats_timer: " << ec.message() << std::endl;
         return;
      }
      
      std::cout << "Current number of sessions: "
                << stats.number_of_sessions
                << std::endl;

      do_stats_logger();
   };

   stats_timer.async_wait(handler);
}

}

