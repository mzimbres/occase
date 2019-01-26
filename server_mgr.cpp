#include "server_mgr.hpp"

#include <mutex>

#include "resp.hpp"
#include "menu.hpp"
#include "server_session.hpp"

namespace
{
 
// TODO: Remove this global once there is a solution to the logger.
std::mutex m;

}

namespace rt
{

server_mgr::server_mgr(server_mgr_cf cf)
: signals(ioc, SIGINT, SIGTERM)
, timeouts(cf.timeouts)
, db(cf.redis_cf, ioc)
, stats_timer(ioc)
{
   net::post(ioc, [this]() {init();});
}

void server_mgr::init()
{
   auto const sig_handler = [this](auto const& ec, auto n)
   {
      // TODO: Verify ec here?
      std::cout << "\nBeginning the shutdown operations ..."
                << std::endl;

      shutdown();
   };

   signals.async_wait(sig_handler);

   do_stats_logger();

   auto const handler = [this]( auto const& ec
                              , auto const& data
                              , auto const& req)
   {
      redis_on_msg_handler(ec, data, req);
   };

   db.set_on_msg_handler(handler);

   db.async_retrieve_menu();
   db.run();
}

void
server_mgr::redis_on_msg_handler( boost::system::error_code const& ec
                                , std::vector<std::string> const& data
                                , redis::req_data const& req)
{
   if (ec) {
      std::cout << "pub_handler: " << ec.message() << std::endl;
      return;
   }

   if (req.cmd == redis::request::retrieve_msgs) {
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

   if (req.cmd == redis::request::get_menu) {
      assert(std::size(data) == 1);
      auto const j_menu = json::parse(data.back());
      menus = j_menu.at("menus").get<std::vector<menu_elem>>();
      auto const menu_codes = menu_elems_to_codes(menus);
      auto const comb_codes = channel_codes(menu_codes, menus);
      for (auto const& gc : comb_codes) {
         //std::cout << "Creating channel " << gc << std::endl;
         auto const new_group = channels.insert({gc, {}});
         if (!new_group.second) {
            std::cout << "Channel " << gc << " already exists."
                      << std::endl;
         }
      }

      std::cout << "Number of channels created: " << std::size(channels)
                << std::endl;
   }

   if (req.cmd == redis::request::unsolicited_publish) {
      assert(std::size(data) == 1);
      auto const j = json::parse(data.front());
      auto const to = j.at("to").get<std::string>();
      auto const g = channels.find(to);
      if (g == std::end(channels)) {
         // Should not happen as the group is checked on
         // on_user_group:msg before being sent to redis for broadcast.
         // TODO: Handle this error.
         assert(false);
         return;
      }

      g->second.broadcast(data.front());
      return;
   }

   if (req.cmd == redis::request::unsolicited_key_not) {
      if (data.back() == "rpush") {
         assert(data.front() == "message");
         assert(std::size(data) == 3);

         // TODO: Read the user id from the req struct. First
         // implement tests.
         auto const n = data[1].rfind(":");
         assert(n != std::string::npos);

         db.async_retrieve_msgs(data[1].substr(n + 1));

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
   resp["menus"] = menus;
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

   db.subscribe_to_chat_msgs(s->get_id());

   json resp;
   resp["cmd"] = "auth_ack";
   resp["result"] = "ok";

   auto const user_versions =
      j.at("menu_versions").get<std::vector<int>>();
   auto const server_versions = read_versions(menus);

   auto const b =
      std::lexicographical_compare( std::begin(user_versions)
                                  , std::end(user_versions)
                                  , std::begin(server_versions)
                                  , std::end(server_versions)
                                  );

   if (b)
      resp["menus"] = menus;

   s->send(resp.dump());

   return ev_res::login_ok;
}

ev_res
server_mgr::on_code_confirmation( json const& j
                                , std::shared_ptr<server_session> s)
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

   db.subscribe_to_chat_msgs(s->get_id());

   json resp;
   resp["cmd"] = "code_confirmation_ack";
   resp["result"] = "ok";
   s->send(resp.dump());
   return ev_res::code_confirmation_ok;
}

ev_res
server_mgr::on_subscribe(json const& j, std::shared_ptr<server_session> s)
{
   auto const codes =
      j.at("channels").get<std::vector<std::vector<std::vector<int>>>>();

   auto const comb_codes = channel_codes(codes, menus);

   auto n_channels = 0;
   for (auto const& o : comb_codes) {
      auto const g = channels.find(o);
      if (g == std::end(channels)) {
         std::cout << "Cannot find channel " << o << std::endl;
         continue;
      }

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
   // The publish command has the form [[1, 2], [2, 3, 4], [1, 2]]
   // Where each array in the outermost array refers to one menu.
   //auto const to = j.at("to").get<std::vector<std::vector<int>>>();
   auto const to = j.at("to").get<std::string>();

   // Now we want to form the hash codes obeying the the menu filter
   // depth, so for example if depth is 2 and the array is
   //
   //   [1, 2, 3, 4]
   //
   // the hash shall consider only the first two elements. We have to
   // combine all arrays each with its own depth.
   //if (std::size(menus) != std::size(to))
   //   return ev_res::publish_fail;

   auto const g = channels.find(to);
   if (g == std::end(channels)) {
      // This is a non-existing channel. Perhaps the json command was
      // sent with the wrong information signaling a logic error in
      // the app. Sending a fail ack back to the app is useful to
      // debug it?
      json resp;
      resp["cmd"] = "publish_ack";
      resp["result"] = "fail";
      resp["id"] = j.at("id").get<int>();
      s->send(resp.dump());
      return ev_res::publish_fail;
   }

   db.publish_menu_msg(std::move(msg));

   json ack;
   ack["cmd"] = "publish_ack";
   ack["result"] = "ok";
   ack["id"] = j.at("id").get<int>();
   s->send(ack.dump());
   return ev_res::publish_ok;
}

void server_mgr::release_auth_session(std::string const& id)
{
   // TODO: This function is called on the destructor on the server
   // session. Think where should we catch exceptions.

   auto const match = sessions.find(id);
   if (match == std::end(sessions)) {
      // This is a bug, all autheticated sessions should be in the
      // sessions map.
      assert(false);
      return;
   }

   sessions.erase(match); // We do not need the return value.
   db.unsubscribe_to_chat_msgs(id);
}

ev_res
server_mgr::on_user_msg( std::string msg, json const& j
                       , std::shared_ptr<server_session> s)
{
   // TODO: Search the sessions map if the user is online and in this
   // node and send him his message directly to avoid overloading the
   // redis server. This would be a big optimization in the case of
   // small number of nodes.

   db.async_store_chat_msg(s->get_id(), std::move(msg));

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

   db.disconnect();

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

void server_mgr::run() noexcept
{
   try {
      ioc.run();
   } catch (std::exception const& e) {
     std::cout << e.what() << std::endl;
   }
}

ev_res
server_mgr::on_message( std::shared_ptr<server_session> s, std::string msg)
{
   auto const j = json::parse(msg);
   auto const cmd = j.at("cmd").get<std::string>();

   if (s->is_waiting_auth()) {
      if (cmd == "register")
         return on_register(j, s);

      if (cmd == "auth")
         return on_login(j, s);

      std::cerr << "Server: Unknown command " << cmd << std::endl;
      return ev_res::unknown;
   }

   if (s->is_waiting_code()) {
      if (cmd == "code_confirmation")
         return on_code_confirmation(j, s);

      std::cerr << "Server: Unknown command " << cmd << std::endl;
      return ev_res::unknown;
   }

   if (s->is_auth()) {
      if (cmd == "subscribe")
         return on_subscribe(j, s);

      if (cmd == "publish")
         return on_publish(std::move(msg), j, s);

      if (cmd == "user_msg")
         return on_user_msg(std::move(msg), j, s);

      if (cmd == "unsubscribe")
         return on_unsubscribe(j, s);

      std::cerr << "Server: Unknown command " << cmd << std::endl;
      return ev_res::unknown;
   }

   std::cerr << "Server: Unknown command " << cmd << std::endl;
   return ev_res::unknown;
}

}

