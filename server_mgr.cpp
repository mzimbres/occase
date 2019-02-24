#include "server_mgr.hpp"

#include <mutex>
#include <chrono>
#include <iterator>
#include <algorithm>

#include "menu.hpp"
#include "server_session.hpp"
#include "json_utils.hpp"

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
, ch_cleanup_freq(cf.channel_cleanup_frequency)
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
server_mgr::on_redis_get_menu(std::vector<std::string> const& data)
{
   assert(std::size(data) == 1);
   auto const j_menu = json::parse(data.back());

   // TODO: Check if the menus have the correct number of elements and
   // the correct depth for each element.
   menus = j_menu.at("menus").get<std::vector<menu_elem>>();
   auto const menu_codes = menu_elems_to_codes(menus);
   auto const arrays = channel_codes(menu_codes, menus);
   std::vector<std::uint64_t> comb_codes;
   std::transform( std::begin(arrays), std::end(arrays)
                 , std::back_inserter(comb_codes)
                 , [this](auto const& o) {
                   return convert_to_channel_code(o);});

   int failed_channel_creation = 0;
   for (auto const& gc : comb_codes) {
      auto const new_group =
         channels.insert({gc, {}});
      if (!new_group.second) {
         ++failed_channel_creation;
      }
   }

   std::cout << "Channels created: " << std::size(channels)
             << std::endl;

   std::cout << "Number of already existing channels: "
             << failed_channel_creation
             << std::endl;
}

void
server_mgr::on_redis_unsol_pub(std::vector<std::string> const& data)
{
   assert(std::size(data) == 1);
   auto const j = json::parse(data.front());
   auto item = j.get<pub_item>();
   auto const code = convert_to_channel_code(item.to);
   auto const g = channels.find(code);
   if (g == std::end(channels)) {
      // Should not happen as the group is checked on on_publish:msg
      // before being sent to redis for broadcast.
      assert(false);
      return;
   }

   g->second.broadcast(item);
}

void
server_mgr::on_redis_unsol_key_not(
   std::vector<std::string> const& data, redis::req_data const& req)
{
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

void
server_mgr::on_redis_retrieve_msgs(
   std::vector<std::string> const& data, redis::req_data const& req)
{
   assert(std::size(data) == 1);
   assert(!std::empty(data.back()));

   //std::cout << req.user_id << " ===> " << data.back() << std::endl;
   auto const match = sessions.find(req.user_id);
   if (match == std::end(sessions)) {
      std::cout << "kdkkdkdkkdkdkdkddk" << std::endl;
      // TODO: The user went offline. We have to enqueue the
      // message again. Rethink this.
      return;
   }

   if (auto s = match->second.lock()) {
      //std::cout << "" << data.back() << std::endl;
      s->send(data.back(), true);
      return;
   }
   
   // The user went offline but the session was not removed from
   // the map. This is perhaps not a bug but undesirable as we
   // do not have garbage collector for expired sessions.
   assert(false);
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

   switch (req.cmd)
   {
      case redis::request::retrieve_msgs:
         on_redis_retrieve_msgs(data, req);
         break;

      case redis::request::get_menu:
         on_redis_get_menu(data);
         break;

      case redis::request::unsolicited_publish:
         on_redis_unsol_pub(data);
         break;

      case redis::request::unsolicited_key_not:
         on_redis_unsol_key_not(data, req);
         break;

      default:
         break;
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
      s->send(resp.dump(), false);
      return ev_res::register_fail;
   }

   s->set_id(from);

   // TODO: Use a random number generator with six digits.
   s->set_code("8347");

   json resp;
   resp["cmd"] = "register_ack";
   resp["result"] = "ok";
   resp["menus"] = menus;
   s->send(resp.dump(), false);
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
      s->send(resp.dump(), false);
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

   s->send(resp.dump(), false);

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
      s->send(resp.dump(), false);
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
   s->send(resp.dump(), false);
   return ev_res::code_confirmation_ok;
}

ev_res
server_mgr::on_subscribe(json const& j, std::shared_ptr<server_session> s)
{
   auto const codes =
      j.at("channels").get<std::vector<std::vector<std::vector<int>>>>();

   auto const arrays = channel_codes(codes, menus);
   std::vector<std::uint64_t> ch_codes;
   std::transform( std::begin(arrays), std::end(arrays)
                 , std::back_inserter(ch_codes)
                 , [this](auto const& o) {
                   return convert_to_channel_code(o);});

   auto n_channels = 0;
   std::vector<pub_item> items;
   for (auto const& o : ch_codes) {
      auto const g = channels.find(o);
      if (g == std::end(channels)) {
         std::cout << "Cannot find channel " << o << std::endl;
         continue;
      }

      g->second.retrieve_pub_items(0, std::back_inserter(items));
      g->second.add_member(s->get_proxy_session(true), ch_cleanup_freq);
      ++n_channels;
   }

   std::cout << "Size of retrieved items: "
             << std::size(items) << std::endl;

   json resp;
   resp["cmd"] = "subscribe_ack";
   resp["result"] = "ok";
   resp["count"] = n_channels;
   resp["items"] = items;
   s->send(resp.dump(), false);
   return ev_res::subscribe_ok;
}

ev_res
server_mgr::on_publish(json j, std::shared_ptr<server_session> s)
{
   // Though the publish command is vectorial allowing the delivery of
   // meny items at the same time, the app is required to send only
   // one item. The reasons are
   //
   // 1. We want to ack every publish. This can be handled by making
   //    the ack to also be vectorial. But this makes the code more
   //    difficult.
   // 2. Each publish item needs an id which at the moment is a
   //    timestamp. Maybe if we change this to be a centralized
   //    counter, we could implement vectorized publish acks.
   // 3. We want to test that each individual item has a correct
   //    channel code. This also makes the code a bit more
   //    complicated.
   auto items = j.at("items").get<std::vector<pub_item>>();
   assert(std::size(items) == 1);

   // The channel code has the form [[1, 2], [2, 3, 4], [1, 2]]
   // where each array in the outermost array refers to one menu.
   auto const code = convert_to_channel_code(items.front().to);

   auto const g = channels.find(code);
   if (g == std::end(channels)) {
      std::cout << code << std::endl;
      // This is a non-existing channel. Perhaps the json command was
      // sent with the wrong information signaling a logic error in
      // the app. Sending a fail ack back to the app is useful to
      // debug it? See redis::request::unsolicited_publish Enable only
      // for debug. But ... It may prevent the traffic of invalid
      // messages in redis. But invalid messages should never happen.
      json resp;
      resp["cmd"] = "publish_ack";
      resp["result"] = "fail";
      s->send(resp.dump(), false);
      return ev_res::publish_fail;
   }

   using ms_type = std::chrono::milliseconds;

   auto const tse = std::chrono::system_clock::now().time_since_epoch();
   auto const ms = std::chrono::duration_cast<ms_type>(tse).count();

   items.front().id = ms;
   json const j_item = items.front();
   db.publish_menu_msg(j_item.dump());

   json ack;
   ack["cmd"] = "publish_ack";
   ack["result"] = "ok";
   ack["id"] = ms;
   s->send(ack.dump(), false);
   return ev_res::publish_ok;
}

void server_mgr::on_session_dtor( std::string const& id
                                , std::deque<std::string> msgs)
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

   //std::cout << msg << std::endl;
   auto const to = j.at("to").get<std::string>();
   db.async_store_chat_msg(to, std::move(msg));

   json ack;
   ack["cmd"] = "user_msg_server_ack";
   ack["result"] = "ok";
   s->send(ack.dump(), false);
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
         return on_publish(std::move(j), s);

      if (cmd == "user_msg")
         return on_user_msg(std::move(msg), j, s);

      std::cerr << "Server: Unknown command " << cmd << std::endl;
      return ev_res::unknown;
   }

   std::cerr << "Server: Unknown command " << cmd << std::endl;
   return ev_res::unknown;
}

}

