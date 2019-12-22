#include "redis.hpp"

#include <boost/core/ignore_unused.hpp>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include "logger.hpp"

using json = nlohmann::json;
using namespace aedis;

namespace occase
{

redis::redis(config const& cfg, net::io_context& ioc)
: cfg_(cfg)
, ss_menu_sub {ioc, cfg.ss_cfg1, "db-menu_sub"}
, ss_menu_pub {ioc, cfg.ss_cfg1, "db-menu_pub"}
, ss_chat_sub {ioc, cfg.ss_cfg2, "db-chat_sub"}
, ss_chat_pub {ioc, cfg.ss_cfg2, "db-chat_pub"}
, worker_handler([](auto const& data, auto const& req) {})
{
   if (!cfg.is_valid())
      throw std::runtime_error("Invalid redis user fields.");

   // Sets on connection handlers.
   auto on_conn_a = [this]() { on_menu_sub_conn(); };
   auto on_conn_b = [this]() { on_menu_pub_conn(); };
   auto on_conn_c = [this]() { on_chat_sub_conn(); };
   auto on_conn_d = [this]() { on_chat_pub_conn(); };

   ss_menu_sub.set_on_conn_handler(on_conn_a);
   ss_menu_pub.set_on_conn_handler(on_conn_b);
   ss_chat_sub.set_on_conn_handler(on_conn_c);
   ss_chat_pub.set_on_conn_handler(on_conn_d);

   auto on_msg_a = [this](auto const& ec, auto data)
      { on_menu_sub(ec, std::move(data)); };
   auto on_msg_b = [this](auto const& ec, auto data)
      { on_menu_pub(ec, std::move(data)); };
   auto on_msg_c = [this](auto const& ec, auto data)
      { on_user_sub(ec, std::move(data)); };
   auto on_msg_d = [this](auto const& ec, auto data)
      { on_user_pub(ec, std::move(data)); };

   ss_menu_sub.set_msg_handler(on_msg_a);
   ss_menu_pub.set_msg_handler(on_msg_b);
   ss_chat_sub.set_msg_handler(on_msg_c);
   ss_chat_pub.set_msg_handler(on_msg_d);
}

void redis::on_menu_sub_conn()
{
   ss_menu_sub.send(subscribe(cfg_.menu_channel_key));
}

void redis::on_menu_pub_conn()
{
   worker_handler({}, {events::menu_connect, {}});
}

void redis::on_chat_sub_conn()
{
}

void redis::on_chat_pub_conn()
{
}

void redis::retrieve_channels()
{
   ss_menu_pub.send(get(cfg_.channels_key));
   menu_pub_queue.push(events::menu);
}

void redis::on_menu_sub( boost::system::error_code const& ec
                        , std::vector<std::string> data)
{
   if (ec) {
      log::write(log::level::debug, "redis::on_menu_sub: {0}.", ec.message());
      return;
   }

   assert(!std::empty(data));

   if (data.front() != "message")
      return;

   assert(std::size(data) == 3);

   if (data[1] == cfg_.menu_channel_key) {
      std::swap(data.front(), data.back());
      data.resize(1);
      worker_handler(std::move(data), {events::channel_post, {}});
      return;
   }
}

void redis::run()
{
   ss_menu_sub.run();
   ss_menu_pub.run();
   ss_chat_sub.run();
   ss_chat_pub.run();
}

void
redis::on_menu_pub( boost::system::error_code const& ec
                   , std::vector<std::string> data)
{
   if (ec) {
      log::write( log::level::debug
                , "redis::on_menu_pub: {0}."
                , ec.message());

      return;
   }

   // This session is not subscribed to any unsolicited message.
   assert(!std::empty(menu_pub_queue));

   if (menu_pub_queue.front() == events::remove_post) {
      assert(!std::empty(data));

      // There are two ways a post can be deleted, either by the user
      // or if it is an innapropriate post, by the person monitoring
      // them. This makes it possible that the deletion returns zero,
      // which means some of them deleted it first.
      auto const n = std::stoi(data.front());
      boost::ignore_unused(n);
      assert(n == 0 || n == 1);
   }

   //if (menu_pub_queue.front() != events::posts) {
   //   assert(!std::empty(data));
   //}

   if (menu_pub_queue.front() == events::ignore) {
      menu_pub_queue.pop();
      return;
   }

   worker_handler(std::move(data), {menu_pub_queue.front(), {}});
   menu_pub_queue.pop();
}

void
redis::on_user_pub( boost::system::error_code const& ec
                   , std::vector<std::string> data)
{
   if (ec) {
      log::write( log::level::debug
                , "redis::on_user_pub: {0}."
                , ec.message());
      return;
   }

   if (std::empty(data)) {
      chat_pub_queue.pop();
      return;
   }

   // This session is not subscribed to any unsolicited message.
   assert(!std::empty(chat_pub_queue));

   if (chat_pub_queue.front().req == events::get_chat_msgs) {
      response const item { events::chat_messages
                          , std::move(chat_pub_queue.front().user_id)};

      // We expect at least one element in this vector, which corresponds
      // to the case where there where no chat messages and the user was
      // not registered, where the del command returns 0.
      assert(!std::empty(data));
      data.pop_back();
      worker_handler(std::move(data), item);
   }

   chat_pub_queue.pop();
}

void
redis::on_user_sub( boost::system::error_code const& ec
                   , std::vector<std::string> data)
{
   if (ec) {
      log::write( log::level::debug
                , "redis::on_user_sub: {0}."
                , ec.message());
      return;
   }

   // Notifications that arrive here have the form.
   //
   //    message pc:6 {"cmd":"presence","from":"5","to":"6","type":"writing"} 
   //    message __keyspace@0__:chat:6 rpush 
   //    message __keyspace@0__:chat:6 expire 
   //    message __keyspace@0__:chat:6 del 
   //
   // We are only interested in rpush and presence messages whose
   // prefix is given by cfg_.presence_channel_prefix.
   //
   // Sometimes we will also receive other kind of message like OK when we
   // send the quit command and we have to filter them too. 

   if (std::size(data) != 3)
      return;

   if (data.back() == "rpush" && data.front() == "message") {
      auto const pos = std::size(cfg_.user_notify_prefix);
      auto const user_id = data[1].substr(pos);
      assert(!std::empty(user_id));
      retrieve_chat_msgs(user_id);
      return;
   }

   auto const size = std::size(cfg_.presence_channel_prefix);
   auto const r = data[1].compare(0, size, cfg_.presence_channel_prefix);
   if (data.front() == "message" && r == 0) {
      auto const user_id = data[1].substr(size);
      std::swap(data.front(), data.back());
      data.resize(1);
      worker_handler(std::move(data), {events::presence, user_id});
      return;
   }
}

void redis::retrieve_chat_msgs(std::string const& user_id)
{
   auto const key = cfg_.chat_msg_prefix + user_id;
   auto cmd = multi()
            + lrange(key)
            + del(key)
            + exec();

   ss_chat_pub.send(std::move(cmd));
   chat_pub_queue.push({events::ignore, {}});
   chat_pub_queue.push({events::ignore, {}});
   chat_pub_queue.push({events::ignore, {}});
   chat_pub_queue.push({events::get_chat_msgs, user_id});
}

void redis::on_user_online(std::string const& id)
{
   ss_chat_sub.send(subscribe(cfg_.user_notify_prefix + id));
   ss_chat_sub.send(subscribe(cfg_.presence_channel_prefix + id));
}

void redis::on_user_offline(std::string const& id)
{
   ss_chat_sub.send(unsubscribe(cfg_.user_notify_prefix + id));
   ss_chat_sub.send(unsubscribe(cfg_.presence_channel_prefix + id));
}

void redis::post(std::string const& msg, int id)
{
   auto cmd = multi()
            + zadd(cfg_.posts_key, id, msg)
            + publish(cfg_.menu_channel_key, msg)
            + exec();

   ss_menu_pub.send(std::move(cmd));
   menu_pub_queue.push(events::ignore);
   menu_pub_queue.push(events::ignore);
   menu_pub_queue.push(events::ignore);
   menu_pub_queue.push(events::post);
}

void redis::remove_post(int id, std::string const& msg)
{
   auto cmd = multi()
            + zremrangebyscore(cfg_.posts_key, id)
            + publish(cfg_.menu_channel_key, msg)
            + exec();

   ss_menu_pub.send(std::move(cmd));
   menu_pub_queue.push(events::ignore);
   menu_pub_queue.push(events::ignore);
   menu_pub_queue.push(events::ignore);
   menu_pub_queue.push(events::remove_post);
}

void redis::request_post_id()
{
   ss_menu_pub.send(incr(cfg_.post_id_key));
   menu_pub_queue.push(events::post_id);
}

void redis::request_user_id()
{
   ss_menu_pub.send(incr(cfg_.user_id_key));
   menu_pub_queue.push(events::user_id);
}

void
redis::register_user( std::string const& user
                    , std::string const& pwd
                    , int n_allowed_posts
                    , std::chrono::seconds deadline)
{
   auto const key =  cfg_.user_data_prefix_key + user;

   std::initializer_list<std::string> par
   { cfg_.ufields.password,  pwd
   , cfg_.ufields.allowed, std::to_string(n_allowed_posts)
   , cfg_.ufields.remaining, std::to_string(n_allowed_posts)
   , cfg_.ufields.deadline, std::to_string(deadline.count())
   };

   ss_menu_pub.send(hset(key, par));
   menu_pub_queue.push(events::register_user);
}

void redis::retrieve_user_data(std::string const& user)
{
   auto const key =  cfg_.user_data_prefix_key + user;
   auto l =
   { cfg_.ufields.password
   , cfg_.ufields.allowed
   , cfg_.ufields.remaining
   , cfg_.ufields.deadline};

   ss_menu_pub.send(hmget(key, l));
   menu_pub_queue.push(events::user_data);
}

void
redis::update_post_deadline( std::string const& user
                           , int n_allowed_posts
                           , std::chrono::seconds deadline)
{
   auto const key =  cfg_.user_data_prefix_key + user;

   std::initializer_list<std::string> l =
   { cfg_.ufields.allowed, std::to_string(n_allowed_posts)
   , cfg_.ufields.remaining, std::to_string(n_allowed_posts)
   , cfg_.ufields.deadline, std::to_string(deadline.count())};

   ss_menu_pub.send(hset(key, l));
   menu_pub_queue.push(events::update_post_deadline);
}

void
redis::update_remaining( std::string const& user_id
                       , int remaining)
{
   auto const key =  cfg_.user_data_prefix_key + user_id;

   std::initializer_list<std::string> l =
   { cfg_.ufields.remaining, std::to_string(remaining)};

   ss_menu_pub.send(hset(key, l));
   menu_pub_queue.push(events::ignore);
}

void redis::retrieve_posts(int begin)
{
   log::write( log::level::debug
             , "redis::retrieve_posts({0})."
             , begin);

   ss_menu_pub.send(zrangebyscore(cfg_.posts_key, begin, -1));
   menu_pub_queue.push(events::posts);
}

void redis::disconnect()
{
   ss_menu_sub.disable_reconnect();
   ss_menu_pub.disable_reconnect();
   ss_chat_sub.disable_reconnect();
   ss_chat_pub.disable_reconnect();

   ss_menu_sub.send(quit());
   ss_menu_pub.send(quit());
   menu_pub_queue.push(events::ignore);

   ss_chat_sub.send(quit());
   ss_chat_pub.send(quit());
   chat_pub_queue.push({events::ignore, {}});
}

void redis::send_presence(std::string id, std::string msg)
{
   // The channel on which presence messages are sent has the
   // following form.

   auto const channel = cfg_.presence_channel_prefix + id;
   ss_chat_pub.send(publish(channel, msg));
   chat_pub_queue.push({events::ignore, {}});
}

void redis::
publish_token( std::string const& id
             , std::string const& token)
{
   json jtoken;
   jtoken["id"] = cfg_.chat_msg_prefix + id;
   jtoken["token"] = token;

   ss_chat_pub.send(publish(cfg_.token_channel, jtoken.dump()));
   chat_pub_queue.push({events::ignore, {}});
}

}

