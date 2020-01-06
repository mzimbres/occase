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
, ss_post_sub_ {ioc, cfg.ss_post, "db-post-sub"}
, ss_post_pub_ {ioc, cfg.ss_post, "db-post-pub"}
, ss_msgs_sub_ {ioc, cfg.ss_msgs, "db-msgs-sub"}
, ss_msgs_pub_ {ioc, cfg.ss_msgs, "db-msgs-pub"}
, ss_user_pub_ {ioc, cfg.ss_user, "db-user-pub"}
, ev_handler_([](auto const& data, auto const& req) {})
{
   if (!cfg.is_valid())
      throw std::runtime_error("Invalid redis user fields.");

   // Sets on connection handlers.
   auto on_conn_a = [this]() { on_post_sub_conn(); };
   auto on_conn_b = [this]() { on_post_pub_conn(); };
   auto on_conn_c = [this]() { on_msgs_sub_conn(); };
   auto on_conn_d = [this]() { on_msgs_pub_conn(); };
   auto on_conn_e = [this]() { on_user_pub_conn(); };

   ss_post_sub_.set_on_conn_handler(on_conn_a);
   ss_post_pub_.set_on_conn_handler(on_conn_b);
   ss_msgs_sub_.set_on_conn_handler(on_conn_c);
   ss_msgs_pub_.set_on_conn_handler(on_conn_d);
   ss_user_pub_.set_on_conn_handler(on_conn_e);

   auto on_ev_a = [this](auto const& ec, auto data)
      { on_post_sub(ec, std::move(data)); };
   auto on_ev_b = [this](auto const& ec, auto data)
      { on_post_pub(ec, std::move(data)); };
   auto on_ev_c = [this](auto const& ec, auto data)
      { on_msgs_sub(ec, std::move(data)); };
   auto on_ev_d = [this](auto const& ec, auto data)
      { on_msgs_pub(ec, std::move(data)); };
   auto on_ev_e = [this](auto const& ec, auto data)
      { on_user_pub(ec, std::move(data)); };

   ss_post_sub_.set_msg_handler(on_ev_a);
   ss_post_pub_.set_msg_handler(on_ev_b);
   ss_msgs_sub_.set_msg_handler(on_ev_c);
   ss_msgs_pub_.set_msg_handler(on_ev_d);
   ss_user_pub_.set_msg_handler(on_ev_e);
}

void redis::on_post_sub_conn()
{
   ss_post_sub_.send(subscribe(cfg_.menu_channel_key));
}

void redis::on_post_pub_conn()
{
   ev_handler_({}, {events::post_connect, {}});
}

void redis::on_msgs_sub_conn()
{
}

void redis::on_msgs_pub_conn()
{
}

void redis::on_user_pub_conn()
{
}

void redis::retrieve_channels()
{
   ss_post_pub_.send(get(cfg_.channels_key));
   post_pub_queue_.push(events::channels);
}

void redis::on_post_sub( boost::system::error_code const& ec
                        , std::vector<std::string> data)
{
   if (ec) {
      log::write( log::level::debug
                , "redis::on_post_sub: {0}."
                , ec.message());
      return;
   }

   assert(!std::empty(data));

   if (data.front() != "message")
      return;

   assert(std::size(data) == 3);

   if (data[1] == cfg_.menu_channel_key) {
      std::swap(data.front(), data.back());
      data.resize(1);
      ev_handler_(std::move(data), {events::channel_post, {}});
      return;
   }
}

void redis::run()
{
   ss_post_sub_.run();
   ss_post_pub_.run();
   ss_msgs_sub_.run();
   ss_msgs_pub_.run();
   ss_user_pub_.run();
}

void
redis::on_post_pub( boost::system::error_code const& ec
                   , std::vector<std::string> data)
{
   if (ec) {
      log::write( log::level::debug
                , "redis::on_post_pub: {0}."
                , ec.message());

      return;
   }

   // This session is not subscribed to any unsolicited message.
   assert(!std::empty(post_pub_queue_));

   if (post_pub_queue_.front() == events::remove_post) {
      assert(!std::empty(data));

      // There are two ways a post can be deleted, either by the user
      // or if it is an innapropriate post, by the person monitoring
      // them. This makes it possible that the deletion returns zero,
      // which means some of them deleted it first.
      auto const n = std::stoi(data.front());
      boost::ignore_unused(n);
      assert(n == 0 || n == 1);
   }

   //if (post_pub_queue_.front() != events::posts) {
   //   assert(!std::empty(data));
   //}

   if (post_pub_queue_.front() == events::ignore)
      return post_pub_queue_.pop();

   ev_handler_(std::move(data), {post_pub_queue_.front(), {}});
   post_pub_queue_.pop();
}

void
redis::on_msgs_pub( boost::system::error_code const& ec
                   , std::vector<std::string> data)
{
   if (ec) {
      log::write( log::level::debug
                , "redis::on_msgs_pub: {0}."
                , ec.message());
      return;
   }

   if (std::empty(data))
      return msgs_pub_queue_.pop();

   // This session is not subscribed to any unsolicited message.
   assert(!std::empty(msgs_pub_queue_));

   if (msgs_pub_queue_.front().req == events::user_msgs_priv) {
      response const item { events::user_messages
                          , std::move(msgs_pub_queue_.front().user_id)};

      // We expect at least one element in this vector, which corresponds
      // to the case where there where no chat messages and the user was
      // not registered, where the del command returns 0.
      assert(!std::empty(data));
      data.pop_back();
      ev_handler_(std::move(data), item);
   }

   msgs_pub_queue_.pop();
}

void redis::
on_user_pub( boost::system::error_code const& ec
           , std::vector<std::string> data)
{
   if (ec) {
      log::write( log::level::debug
                , "redis::on_user_pub: {0}."
                , ec.message());
      return;
   }

   assert(!std::empty(user_pub_queue_));

   if (user_pub_queue_.front() == events::ignore)
      return user_pub_queue_.pop();

   ev_handler_(std::move(data), {user_pub_queue_.front(), {}});
   user_pub_queue_.pop();
}

void
redis::on_msgs_sub( boost::system::error_code const& ec
                   , std::vector<std::string> data)
{
   if (ec) {
      log::write( log::level::debug
                , "redis::on_msgs_sub: {0}."
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
      retrieve_messages(user_id);
      return;
   }

   auto const size = std::size(cfg_.presence_channel_prefix);
   auto const r = data[1].compare(0, size, cfg_.presence_channel_prefix);
   if (data.front() == "message" && r == 0) {
      auto const user_id = data[1].substr(size);
      std::swap(data.front(), data.back());
      data.resize(1);
      ev_handler_(std::move(data), {events::presence, user_id});
      return;
   }
}

void redis::retrieve_messages(std::string const& user_id)
{
   auto const key = cfg_.chat_msg_prefix + user_id;
   auto cmd = multi()
            + lrange(key)
            + del(key)
            + exec();

   ss_msgs_pub_.send(std::move(cmd));
   msgs_pub_queue_.push({events::ignore, {}});
   msgs_pub_queue_.push({events::ignore, {}});
   msgs_pub_queue_.push({events::ignore, {}});
   msgs_pub_queue_.push({events::user_msgs_priv, user_id});
}

void redis::on_user_online(std::string const& id)
{
   ss_msgs_sub_.send(subscribe(cfg_.user_notify_prefix + id));
   ss_msgs_sub_.send(subscribe(cfg_.presence_channel_prefix + id));
}

void redis::on_user_offline(std::string const& id)
{
   ss_msgs_sub_.send(unsubscribe(cfg_.user_notify_prefix + id));
   ss_msgs_sub_.send(unsubscribe(cfg_.presence_channel_prefix + id));
}

void redis::post(std::string const& msg, int id)
{
   auto cmd = multi()
            + zadd(cfg_.posts_key, id, msg)
            + publish(cfg_.menu_channel_key, msg)
            + exec();

   ss_post_pub_.send(std::move(cmd));
   post_pub_queue_.push(events::ignore);
   post_pub_queue_.push(events::ignore);
   post_pub_queue_.push(events::ignore);
   post_pub_queue_.push(events::post);
}

void redis::remove_post(int id, std::string const& msg)
{
   auto cmd = multi()
            + zremrangebyscore(cfg_.posts_key, id)
            + publish(cfg_.menu_channel_key, msg)
            + exec();

   ss_post_pub_.send(std::move(cmd));
   post_pub_queue_.push(events::ignore);
   post_pub_queue_.push(events::ignore);
   post_pub_queue_.push(events::ignore);
   post_pub_queue_.push(events::remove_post);
}

void redis::request_post_id()
{
   ss_post_pub_.send(incr(cfg_.post_id_key));
   post_pub_queue_.push(events::post_id);
}

void redis::request_user_id()
{
   ss_user_pub_.send(incr(cfg_.user_id_key));
   user_pub_queue_.push(events::user_id);
}

void
redis::register_user( std::string const& user
                    , std::string const& pwd
                    , int n_allowed_posts
                    , std::chrono::seconds deadline)
{
   auto const key =  cfg_.user_data_prefix_key + user;

   auto par =
   { cfg_.ufields.password,  pwd
   , cfg_.ufields.allowed, std::to_string(n_allowed_posts)
   , cfg_.ufields.remaining, std::to_string(n_allowed_posts)
   , cfg_.ufields.deadline, std::to_string(deadline.count())
   };

   ss_user_pub_.send(hset(key, par));
   user_pub_queue_.push(events::register_user);
}

void redis::retrieve_user_data(std::string const& user)
{
   auto const key =  cfg_.user_data_prefix_key + user;

   auto l =
   { cfg_.ufields.password
   , cfg_.ufields.allowed
   , cfg_.ufields.remaining
   , cfg_.ufields.deadline};

   ss_user_pub_.send(hmget(key, l));
   user_pub_queue_.push(events::user_data);
}

void
redis::update_post_deadline( std::string const& user
                           , int n_allowed_posts
                           , std::chrono::seconds deadline)
{
   auto const key =  cfg_.user_data_prefix_key + user;

   auto l =
   { cfg_.ufields.allowed, std::to_string(n_allowed_posts)
   , cfg_.ufields.remaining, std::to_string(n_allowed_posts)
   , cfg_.ufields.deadline, std::to_string(deadline.count())};

   ss_post_pub_.send(hset(key, l));
   post_pub_queue_.push(events::update_post_deadline);
}

void
redis::update_remaining( std::string const& user_id
                       , int remaining)
{
   auto const key = cfg_.user_data_prefix_key + user_id;

   auto l = {cfg_.ufields.remaining, std::to_string(remaining)};

   ss_user_pub_.send(hset(key, l));
   user_pub_queue_.push(events::ignore);
}

void redis::retrieve_posts(int begin)
{
   log::write( log::level::debug
             , "redis::retrieve_posts({0})."
             , begin);

   ss_post_pub_.send(zrangebyscore(cfg_.posts_key, begin, -1));
   post_pub_queue_.push(events::posts);
}

void redis::disconnect()
{
   ss_post_sub_.disable_reconnect();
   ss_post_pub_.disable_reconnect();
   ss_msgs_sub_.disable_reconnect();
   ss_msgs_pub_.disable_reconnect();
   ss_user_pub_.disable_reconnect();

   ss_post_sub_.send(quit());
   ss_post_pub_.send(quit());
   post_pub_queue_.push(events::ignore);

   ss_msgs_sub_.send(quit());
   ss_msgs_pub_.send(quit());
   msgs_pub_queue_.push({events::ignore, {}});

   ss_user_pub_.send(quit());
   user_pub_queue_.push(events::ignore);
}

void redis::
send_presence( std::string const& id
             , std::string const& msg)
{
   // The channel on which presence messages are sent has the
   // following form.

   auto const channel = cfg_.presence_channel_prefix + id;
   ss_msgs_pub_.send(publish(channel, msg));
   msgs_pub_queue_.push({events::ignore, {}});
}

void redis::
publish_token( std::string const& id
             , std::string const& token)
{
   json jtoken;
   jtoken["cmd"] = "token";
   jtoken["id"] = cfg_.chat_msg_prefix + id;
   jtoken["token"] = token;

   ss_msgs_pub_.send(publish(cfg_.token_channel, jtoken.dump()));
   msgs_pub_queue_.push({events::ignore, {}});
}

}

