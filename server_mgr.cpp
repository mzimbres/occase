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

server_mgr::server_mgr(server_mgr_cf cf, int id_)
: id(id_)
, ch_cleanup_rate(cf.ch_cleanup_rate)
, ch_max_posts(cf.ch_max_posts)
, ch_max_sub(cf.ch_max_sub)
, max_menu_msg_on_sub(cf.max_menu_msg_on_sub)
, signals(ioc, SIGINT, SIGTERM)
, timeouts(cf.timeouts)
, db(cf.redis_cf, ioc)
, stats_timer(ioc)
{
   net::post(ioc, [this]() {init();});
}

void server_mgr::init()
{
   auto sig_handler = [this](auto const& ec, auto n)
      { shutdown(ec); };

   signals.async_wait(sig_handler);

   do_stats_logger();

   auto handler = [this](auto const& data, auto const& req)
      { on_db_msg_handler(data, req); };

   db.set_on_msg_handler(handler);

   db.async_retrieve_menu();
   db.run();
}

void
server_mgr::on_db_get_menu(std::string const& data)
{
   auto const j_menu = json::parse(data);

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

   std::clog << "Worker " << id << ": " << std::size(channels)
             << " channels created." << std::endl;

   std::clog << "Worker " << id << ": " << failed_channel_creation
             << " channels already existed." << std::endl;
}

void server_mgr::on_db_unsol_pub(std::string const& data)
{
   auto const j = json::parse(data);
   auto item = j.get<pub_item>();
   auto const code = convert_to_channel_code(item.to);
   auto const g = channels.find(code);
   if (g == std::end(channels)) {
      // Should not happen as the group is checked on on_user_publish:msg
      // before being sent to redis for broadcast.
      assert(false);
      return;
   }

   if (item.id > last_menu_msg_id)
      last_menu_msg_id = item.id;
   else
      ++menu_msg_inversions;

   g->second.broadcast(item, ch_max_posts);
}

void
server_mgr::on_db_user_msgs( std::string const& user_id
                           , std::vector<std::string> const& msgs) const
{
   assert(std::size(msgs) == 1);
   assert(!std::empty(msgs.back()));

   auto const match = sessions.find(user_id);
   if (match == std::end(sessions)) {
      // TODO: The user went offline. We have to enqueue the
      // message again. Rethink this.
      return;
   }

   if (auto s = match->second.lock()) {
      //std::cout << "" << msgs.back() << std::endl;
      s->send(msgs.back(), true);
      return;
   }
   
   // The user went offline but the session was not removed from
   // the map. This is perhaps not a bug but undesirable as we
   // do not have garbage collector for expired sessions.
   assert(false);
}

void
server_mgr::on_db_msg_handler( std::vector<std::string> const& data
                             , redis::req_item const& req)
{
   switch (req.req)
   {
      case redis::request::unsol_user_msgs:
         on_db_user_msgs(req.user_id, data);
         break;

      case redis::request::get_menu:
         assert(std::size(data) == 1);
         on_db_get_menu(data.back());
         break;

      case redis::request::unsolicited_publish:
         assert(std::size(data) == 1);
         on_db_unsol_pub(data.back());
         break;

      case redis::request::pub_counter:
         assert(std::size(data) == 1);
         on_db_pub_counter(data.back());
         break;

      case redis::request::publish:
         on_db_publish();
         break;

      default:
         break;
   }
}

void server_mgr::on_db_publish()
{
   pub_wait_queue.pop();
   if (!std::empty(pub_wait_queue))
      db.request_pub_id();
}

void server_mgr::on_db_pub_counter(std::string const& pub_id_str)
{
   auto const pub_id = std::stoi(pub_id_str);
   pub_wait_queue.front().item.id = pub_id;
   json const j_item = pub_wait_queue.front().item;
   db.pub_menu_msg(j_item.dump(), pub_id);

   // It is important that the publisher receives this message before
   // any user sends him a user message about the post. He needs a
   // pub_id to know to which post the user refers to.
   json ack;
   ack["cmd"] = "publish_ack";
   ack["result"] = "ok";
   ack["id"] = pub_wait_queue.front().item.id;
   auto const ack_str = ack.dump();

   if (auto s = pub_wait_queue.front().session.lock()) {
      s->send(ack_str, true);
      return;
   }

   // If we get here the user is not online anymore. This should be a
   // very rare situation since requesting an id takes milliseconds.
   // We can simply store his ack in the database for later retrieval
   // when the user connects again. It shall be difficult to test
   // this.

   std::initializer_list<std::string> param = {ack_str};

   db.store_user_msg( std::move(pub_wait_queue.front().user_id)
                    , std::make_move_iterator(std::begin(param))
                    , std::make_move_iterator(std::end(param)));
}

ev_res
server_mgr::on_user_register( json const& j
                            , std::shared_ptr<server_session> s)
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

ev_res
server_mgr::on_user_login( json const& j
                         , std::shared_ptr<server_session> s)
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

   db.sub_to_user_msgs(s->get_id());

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
server_mgr::on_user_code_confirm( json const& j
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

   db.sub_to_user_msgs(s->get_id());

   json resp;
   resp["cmd"] = "code_confirmation_ack";
   resp["result"] = "ok";
   s->send(resp.dump(), false);
   return ev_res::code_confirmation_ok;
}

ev_res
server_mgr::on_user_subscribe( json const& j
                             , std::shared_ptr<server_session> s)
{
   auto const codes =
      j.at("channels").get<std::vector<std::vector<std::vector<int>>>>();

   auto const arrays = channel_codes(codes, menus);
   std::vector<std::uint64_t> ch_codes;

   auto const converter = [this](auto const& o)
      { return convert_to_channel_code(o);};

   std::transform( std::begin(arrays), std::end(arrays)
                 , std::back_inserter(ch_codes)
                 , converter);

   auto n_channels = 0;
   std::vector<pub_item> items;

   auto func = [&, this](auto const& o)
   {
      auto const g = channels.find(o);
      assert(g != std::end(channels));
      if (g == std::end(channels))
         return;

      // TODO: Change 0 with the latest id the user has received.
      g->second.retrieve_pub_items(0, std::back_inserter(items));
      g->second.add_member( s->get_proxy_session(true)
                          , ch_cleanup_rate);
      ++n_channels;
   };

   // TODO: Update the compiler and use std::for_each_n.
   auto const size = ssize(ch_codes);
   auto const n = ch_max_sub > size ? size : ch_max_sub;
   auto const end = std::begin(ch_codes) + n;
   std::for_each(std::begin(ch_codes), end, func);

   if (ssize(items) > max_menu_msg_on_sub) {
      auto const d = ssize(items) - max_menu_msg_on_sub;
      std::cout << "====> " << d << std::endl;
      // Notice here we want to move the most recent elements to the
      // front of the vector.
      auto comp = [](auto const& a, auto const& b)
         { return a.id > b.id; };

      std::nth_element( std::begin(items)
                      , std::begin(items) + max_menu_msg_on_sub
                      , std::end(items)
                      , comp);

      items.erase( std::begin(items) + max_menu_msg_on_sub
                 , std::end(items));
   }

   //std::cout << "Size of retrieved items: "
   //          << std::size(items) << std::endl;

   json resp;
   resp["cmd"] = "subscribe_ack";
   resp["result"] = "ok";
   resp["count"] = n_channels;
   resp["items"] = items;
   s->send(resp.dump(), false);
   return ev_res::subscribe_ok;
}

ev_res
server_mgr::on_user_publish(json j, std::shared_ptr<server_session> s)
{
   // TODO: Remove the restriction below that the items vector have
   // size one.
   auto items = j.at("items").get<std::vector<pub_item>>();

   // Before we request a new pub id, we have to check that the post
   // is valid and will not be refused later. This prevents the pub
   // items from being incremented on an invalid message. May be
   // overkill since we do not expect the app to contain so severe
   // bugs. Also, the session at this point has been authenticated and
   // can be thrusted.

   // The channel code has the form [[1, 2], [2, 3, 4], [1, 2]]
   // where each array in the outermost array refers to one menu.
   auto const code = convert_to_channel_code(items.front().to);

   auto const g = channels.find(code);
   if (g == std::end(channels) || std::size(items) != 1) {
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

   auto const is_empty = std::empty(pub_wait_queue);

   pub_wait_queue.push({s, std::move(items.front()), items.front().from});

   if (is_empty)
      db.request_pub_id();

   return ev_res::publish_ok;
}

void server_mgr::on_session_dtor( std::string const& id
                                , std::vector<std::string> msgs)
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
   db.unsub_to_user_msgs(id);

   if (!std::empty(msgs)) {
      db.store_user_msg( std::move(id)
                       , std::make_move_iterator(std::begin(msgs))
                       , std::make_move_iterator(std::end(msgs)));
   }
}

ev_res
server_mgr::on_user_msg( std::string msg, json const& j
                       , std::shared_ptr<server_session> s)
{
   // TODO: Search the sessions map if the user is online and in this
   // node and send him his message directly to avoid overloading the
   // redis server. This would be a big optimization in the case of
   // small number of nodes.

   auto to = j.at("to").get<std::string>();

   std::initializer_list<std::string> param = {msg};

   db.store_user_msg( std::move(to)
                    , std::make_move_iterator(std::begin(param))
                    , std::make_move_iterator(std::end(param)));

   json ack;
   ack["cmd"] = "user_msg_server_ack";
   ack["result"] = "ok";
   s->send(ack.dump(), false);
   return ev_res::user_msg_ok;
}

void server_mgr::shutdown(boost::system::error_code const& ec)
{
   // TODO: Verify ec here?
   std::clog << "Shutting down server " << id << std::endl;

   std::clog << "Shutting down " << std::size(sessions) 
             << " user sessions ..." << std::endl;

   auto f = [](auto o)
   {
      if (auto s = o.second.lock())
         s->shutdown();
   };

   std::for_each(std::begin(sessions), std::end(sessions), f);

   db.disconnect();

   stats_timer.cancel();

   std::clog << "Number of inversions: " << menu_msg_inversions
             << std::endl;
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
         return on_user_register(j, s);

      if (cmd == "auth")
         return on_user_login(j, s);

      std::cerr << "Server: Unknown command " << cmd << std::endl;
      return ev_res::unknown;
   }

   if (s->is_waiting_code()) {
      if (cmd == "code_confirmation")
         return on_user_code_confirm(j, s);

      std::cerr << "Server: Unknown command " << cmd << std::endl;
      return ev_res::unknown;
   }

   if (s->is_auth()) {
      if (cmd == "subscribe")
         return on_user_subscribe(j, s);

      if (cmd == "publish")
         return on_user_publish(std::move(j), s);

      if (cmd == "user_msg")
         return on_user_msg(std::move(msg), j, s);

      std::cerr << "Server: Unknown command " << cmd << std::endl;
      return ev_res::unknown;
   }

   std::cerr << "Server: Unknown command " << cmd << std::endl;
   return ev_res::unknown;
}

}

