#include "server_mgr.hpp"

#include <mutex>

#include "resp.hpp"
#include "menu_parser.hpp"
#include "server_session.hpp"

namespace
{
 
// TODO: Remove this global once there is a solution to the logger.
std::mutex m;

}

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
      if (cmd == "register")
         return mgr.on_register(j, s);

      if (cmd == "auth")
         return mgr.on_login(j, s);

      std::cerr << "Server: Unknown command " << cmd << std::endl;
      return ev_res::unknown;
   }

   if (s->is_waiting_code()) {
      if (cmd == "code_confirmation")
         return mgr.on_code_confirmation(j, s);

      std::cerr << "Server: Unknown command " << cmd << std::endl;
      return ev_res::unknown;
   }

   if (s->is_auth()) {
      if (cmd == "subscribe")
         return mgr.on_subscribe(j, s);

      if (cmd == "publish")
         return mgr.on_publish(std::move(msg), j, s);

      if (cmd == "user_msg")
         return mgr.on_user_msg(std::move(msg), j, s);

      if (cmd == "unsubscribe")
         return mgr.on_unsubscribe(j, s);

      std::cerr << "Server: Unknown command " << cmd << std::endl;
      return ev_res::unknown;
   }

   std::cerr << "Server: Unknown command " << cmd << std::endl;
   return ev_res::unknown;
}

server_mgr::server_mgr(server_mgr_cf cf, net::io_context& ioc_)
: ioc(ioc_)
, timeouts(cf.get_timeouts())
, redis_msub(cf.get_redis_session_cf(), ioc)
, redis_ksub(cf.get_redis_session_cf(), ioc)
, redis_pub(cf.get_redis_session_cf(), ioc)
, redis_nms(cf.redis_nms)
, stats_timer(ioc)
{
   auto handler = [this]( auto const& ec, auto const& data
                        , auto const& req)
   { redis_pub_msg_handler(ec, data, req); };

   redis_pub.set_on_msg_handler(handler);
   redis_pub.run();

   // Asynchronously retrieves the menu.
   req_data r
   { request::get_menu
   , gen_resp_cmd(redis_cmd::get, {redis_nms.menu_key})
   , ""//redis_nms.menu_key
   };
   redis_pub.send(std::move(r));
   do_stats_logger();
}

void
server_mgr::redis_group_msg_handler( boost::system::error_code const& ec
                                   , std::vector<std::string> const& data
                                   , req_data const& req)
{
   if (ec) {
      std::cout << "sub_handler: " << ec.message() << std::endl;
      return;
   }

   if (req.cmd == request::unsolicited) {
      assert(data.front() == "message");
      assert(std::size(data) == 3);
      assert(data[1] == redis_nms.menu_channel);

      auto const j = json::parse(data.back());
      auto const to = j.at("to").get<std::string>();
      auto const g = channels.find(to);
      if (g == std::end(channels)) {
         // Should not happen as the group is checked on
         // on_user_group:msg before being sent to redis for broadcast.
         // TODO: Handle this error.
         assert(false);
         return;
      }

      g->second.broadcast(data.back());

      return;
   }
}

void 
server_mgr::redis_key_msg_handler( boost::system::error_code const& ec
                                 , std::vector<std::string> const& data
                                 , req_data const& req)
{
   if (ec) {
      std::cout << "sub_handler: " << ec.message() << std::endl;
      return;
   }

   if (req.cmd == request::unsolicited) {
      if (data.back() == "rpush") {
         assert(data.front() == "message");
         assert(std::size(data) == 3);
         auto const n = data[1].rfind(":");
         assert(n != std::string::npos);

         // We have to retrieve the user message.
         auto const user_id = data[1].substr(n + 1);
         auto const key = redis_nms.msg_prefix + user_id;
         req_data r
         { request::lpop
         , gen_resp_cmd(redis_cmd::lpop, {key})
         , user_id
         };
         redis_pub.send(r);

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
                                 , req_data const& req)
{
   if (ec) {
      std::cout << "pub_handler: " << ec.message() << std::endl;
      return;
   }

   if (req.cmd == request::lpop) {
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

   if (req.cmd == request::get_menu) {
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

      redis_msub.set_on_msg_handler(handler1);
      redis_msub.run();

      req_data r
      { request::subscribe
      , gen_resp_cmd(redis_cmd::subscribe, {redis_nms.menu_channel})
      , ""
      };
      redis_msub.send(std::move(r));
      auto const handler3 = [this]( auto const& ec , auto const& data
                                  , auto const& req)
      { redis_key_msg_handler(ec, data, req); };

      redis_ksub.set_on_msg_handler(handler3);
      redis_ksub.run();
   }
}

ev_res server_mgr::on_register(json const& j, std::shared_ptr<server_session> s)
{
   auto const from = j.at("from").get<std::string>();

   // TODO: Replace this with a query to the database.
   if (sessions.find(from) != std::end(sessions)) {
      // The user already exists in the system.
      json resp;
      resp["cmd"] = "register_ack";
      resp["result"] = "fail";
      s->send(resp.dump());
      return ev_res::register_fail;
   }

   s->set_id(from);

   // TODO: Use a random number generator with six digits.
   s->set_code("8347");

   json resp;
   resp["cmd"] = "register_ack";
   resp["result"] = "ok";
   resp["menu"] = menu;
   s->send(resp.dump());
   return ev_res::register_ok;
}

ev_res server_mgr::on_login(json const& j, std::shared_ptr<server_session> s)
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
      return ev_res::login_fail;
   }

   // TODO: Query the database to validate the session.
   //if (from != s->get_id()) {
   //   // Incorrect id.
   //   json resp;
   //   resp["cmd"] = "auth_ack";
   //   resp["result"] = "fail";
   //   s->send(resp.dump());
   //   return ev_res::login_fail;
   //}

   s->set_id(from);
   s->promote();
   assert(s->is_auth());

   req_data r
   { request::subscribe
   , gen_resp_cmd( redis_cmd::subscribe
                 , { redis_nms.notify_prefix + s->get_id() })
   , ""
   };

   redis_ksub.send(std::move(r));

   json resp;
   resp["cmd"] = "auth_ack";
   resp["result"] = "ok";

   auto const menu_version = j.at("menu_version").get<int>();
   if (menu_version == -1)
      resp["menu"] = menu;

   s->send(resp.dump());

   return ev_res::login_ok;
}

ev_res
server_mgr::on_code_confirmation(json const& j, std::shared_ptr<server_session> s)
{
   auto const from = j.at("from").get<std::string>();
   auto const code = j.at("code").get<std::string>();

   if (code != s->get_code()) {
      json resp;
      resp["cmd"] = "code_confirmation_ack";
      resp["result"] = "fail";
      s->send(resp.dump());
      return ev_res::code_confirmation_fail;
   }

   s->promote();

   // Inserts the user in the system.
   assert(!std::empty(s->get_id()));
   auto const new_user = sessions.insert({from, s});
   assert(s->is_auth());

   // This would be odd. The entry already exists on the index map
   // which means we did something wrong in the register command.
   assert(new_user.second);

   req_data r
   { request::subscribe
   , gen_resp_cmd( redis_cmd::subscribe
                 , {redis_nms.notify_prefix + s->get_id()})
   , ""
   };

   redis_ksub.send(std::move(r));

   json resp;
   resp["cmd"] = "code_confirmation_ack";
   resp["result"] = "ok";
   s->send(resp.dump());
   return ev_res::code_confirmation_ok;
}

ev_res
server_mgr::on_subscribe(json const& j, std::shared_ptr<server_session> s)
{
   auto const codes = j.at("channels").get<std::vector<std::string>>();
   auto const from = s->get_id();

   if (std::empty(codes)) {
      json resp;
      resp["cmd"] = "subscribe_ack";
      resp["result"] = "ok";
      resp["count"] = 0;
      return ev_res::subscribe_ok;
   }

   auto n_channels = 0;
   for (auto const& o : codes) {
      auto const g = channels.find(o);
      if (g == std::end(channels))
         continue;

      g->second.add_member(s);
      ++n_channels;
   }

   json resp;
   resp["cmd"] = "subscribe_ack";
   resp["result"] = "ok";
   resp["count"] = n_channels;
   s->send(resp.dump());
   return ev_res::subscribe_ok;
}

ev_res
server_mgr::on_unsubscribe(json const& j, std::shared_ptr<server_session> s)
{
   auto const codes = j.at("channels").get<std::vector<std::string>>();
   auto const from = s->get_id();

   if (std::empty(codes)) {
      json resp;
      resp["cmd"] = "unsubscribe_ack";
      resp["result"] = "ok";
      resp["count"] = 0;
      return ev_res::subscribe_ok;
   }

   auto n_channels = 0;
   for (auto const& o : codes) {
      auto const g = channels.find(o);
      if (g == std::end(channels))
         continue;

      g->second.remove_member(from);
      ++n_channels;
   }

   json resp;
   resp["cmd"] = "unsubscribe_ack";
   resp["result"] = "ok";
   resp["count"] = n_channels;
   resp["channels"] = codes;
   s->send(resp.dump());
   return ev_res::unsubscribe_ok;
}


ev_res
server_mgr::on_publish( std::string msg, json const& j
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
      resp["cmd"] = "publish_ack";
      resp["result"] = "fail";
      resp["id"] = j.at("id").get<int>();
      s->send(resp.dump());
      return ev_res::publish_fail;
   }

   req_data r
   { request::publish
   , gen_resp_cmd(redis_cmd::publish, {redis_nms.menu_channel, msg})
   , ""
   };

   redis_pub.send(std::move(r));

   json ack;
   ack["cmd"] = "publish_ack";
   ack["result"] = "ok";
   ack["id"] = j.at("id").get<int>();
   s->send(ack.dump());
   return ev_res::publish_ok;
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

   req_data r
   { request::unsubscribe
   , gen_resp_cmd( redis_cmd::unsubscribe
                 , { redis_nms.notify_prefix + id})
   , ""
   };

   redis_ksub.send(std::move(r));
}

ev_res
server_mgr::on_user_msg( std::string msg, json const& j
                       , std::shared_ptr<server_session> s)
{
   // TODO: Search the sessions map if the user is online and in this
   // node and send him his message directly to avoid overloading the
   // redis server. This would be a big optimization in the case of
   // small number of nodes.

   req_data r
   { request::rpush
   , gen_resp_cmd( redis_cmd::rpush
                 , {redis_nms.msg_prefix + s->get_id(), msg})
   , ""
   };

   redis_pub.send(std::move(r));

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

   redis_msub.close();

   std::cout << "Shuting down redis publish session ..."
             << std::endl;

   redis_pub.close();

   std::cout << "Shuting down redis user msg subscribe session ..."
             << std::endl;

   redis_ksub.close();

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
      
      std::lock_guard mut(m);
      std::cout << "Current number of sessions: "
                << stats.number_of_sessions
                << std::endl;

      do_stats_logger();
   };

   stats_timer.async_wait(handler);
}

}

