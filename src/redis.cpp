#include "redis.hpp"

#include <fmt/format.h>

#include "logger.hpp"

namespace rt::redis
{

facade::facade(config const& cfg, net::io_context& ioc, int wid)
: worker_id {wid}
, cfg(cfg.cfg)
, ss_menu_sub {cfg.ss_cfg, ioc, fmt::format("W{0}/db-menu_sub", wid)}
, ss_menu_pub {cfg.ss_cfg, ioc, fmt::format("W{0}/db-menu_pub", wid)}
, ss_chat_sub {cfg.ss_cfg, ioc, fmt::format("W{0}/db-chat_sub", wid)}
, ss_chat_pub {cfg.ss_cfg, ioc, fmt::format("W{0}/db-chat_pub", wid)}
, worker_handler([](auto const& data, auto const& req) {})
{
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

void facade::on_menu_sub_conn()
{
   ss_menu_sub.send(subscribe(cfg.menu_channel_key));
   ss_menu_sub.send(subscribe(cfg.notify_prefix + cfg.menu_key));
}

void facade::on_menu_pub_conn()
{
   worker_handler({}, {request::menu_connect, {}});
}

void facade::on_chat_sub_conn()
{
}

void facade::on_chat_pub_conn()
{
}

void facade::retrieve_menu()
{
   ss_menu_pub.send(get(cfg.menu_key));
   menu_pub_queue.push(request::menu);
}

void facade::on_menu_sub( boost::system::error_code const& ec
                        , std::vector<std::string> data)
{
   if (ec) {
      log( loglevel::debug, "W{0}/facade::on_menu_sub: {1}."
         , worker_id, ec.message());

      return;
   }

   assert(!std::empty(data));

   if (data.front() != "message")
      return;

   assert(std::size(data) == 3);

   if (data[1] == cfg.menu_channel_key) {
      // This is a menu message.
      std::swap(data.front(), data.back());
      data.resize(1);
      worker_handler(std::move(data), {request::unsol_publish, {}});
      return;
   }

   auto const key = cfg.notify_prefix + cfg.menu_key;
   if (data[1] == key) {
      // A menu update has been received.
      ss_menu_pub.send(get(cfg.menu_key));
      menu_pub_queue.push(request::menu);
      return;
   }
}

void facade::run()
{
   ss_menu_sub.run();
   ss_menu_pub.run();
   ss_chat_sub.run();
   ss_chat_pub.run();
}

void
facade::on_menu_pub( boost::system::error_code const& ec
                   , std::vector<std::string> data)
{
   if (ec) {
      log( loglevel::debug, "W{0}/facade::on_menu_pub: {1}."
         , worker_id, ec.message());

      return;
   }

   if (menu_pub_queue.front() == request::remove_post) {
      assert(!std::empty(data));
      assert(std::stoi(data.front()) == 1);
   }

   if (menu_pub_queue.front() != request::posts) {
      assert(!std::empty(data));
   }

   // This session is not subscribed to any unsolicited message.
   assert(!std::empty(menu_pub_queue));

   if (menu_pub_queue.front() == request::ignore) {
      menu_pub_queue.pop();
      return;
   }

   worker_handler(std::move(data), {menu_pub_queue.front(), {}});
   menu_pub_queue.pop();
}

void
facade::on_user_pub( boost::system::error_code const& ec
                   , std::vector<std::string> data)
{
   if (ec) {
      log( loglevel::debug, "W{0}/facade::on_user_sub: {1}."
         , worker_id, ec.message());
      return;
   }

   if (std::empty(data)) {
      chat_pub_queue.pop();
      return;
   }

   // This session is not subscribed to any unsolicited message.
   assert(!std::empty(chat_pub_queue));

   if (chat_pub_queue.front().req == request::get_chat_msgs) {
      req_item const item { request::chat_messages
                          , std::move(chat_pub_queue.front().user_id)};

      worker_handler(std::move(data), item);
   }

   chat_pub_queue.pop();
}

void
facade::on_user_sub( boost::system::error_code const& ec
                   , std::vector<std::string> data)
{
   if (ec) {
      log( loglevel::debug, "W{0}/facade::on_user_sub: {1}."
         , worker_id, ec.message());

      return;
   }

   if (data.back() != "rpush")
      return;

   assert(data.front() == "message");
   assert(std::size(data) == 3);

   auto const pos = std::size(cfg.user_notify_prefix);
   auto user_id = data[1].substr(pos);
   assert(!std::empty(user_id));
   retrieve_chat_msgs(user_id);
}

void facade::retrieve_chat_msgs(std::string const& user_id)
{
   auto const key = cfg.chat_msg_prefix + user_id;
   auto cmd = multi()
            + lrange(key, 0, -1)
            + del(key)
            + exec();

   ss_chat_pub.send(std::move(cmd));
   chat_pub_queue.push({request::ignore, {}});
   chat_pub_queue.push({request::ignore, {}});
   chat_pub_queue.push({request::ignore, {}});
   chat_pub_queue.push({request::get_chat_msgs, user_id});
   chat_pub_queue.push({request::ignore, {}});
}

void facade::subscribe_to_chat_msgs(std::string const& id)
{
   ss_chat_sub.send(subscribe(cfg.user_notify_prefix + id));
}

void facade::unsubscribe_to_chat_msgs(std::string const& id)
{
   ss_chat_sub.send(unsubscribe(cfg.user_notify_prefix + id));
}

void facade::post(std::string const& msg, int id)
{
   auto cmd = multi()
            + zadd(cfg.posts_key, id, msg)
            + publish(cfg.menu_channel_key, msg)
            + exec();

   ss_menu_pub.send(std::move(cmd));
   menu_pub_queue.push(request::ignore);
   menu_pub_queue.push(request::ignore);
   menu_pub_queue.push(request::ignore);
   menu_pub_queue.push(request::post);
}

void facade::remove_post( int id, std::string const& msg)
{
   auto cmd = multi()
            + zremrangebyscore(cfg.posts_key, id)
            + publish(cfg.menu_channel_key, msg)
            + exec();

   ss_menu_pub.send(std::move(cmd));
   menu_pub_queue.push(request::ignore);
   menu_pub_queue.push(request::ignore);
   menu_pub_queue.push(request::ignore);
   menu_pub_queue.push(request::remove_post);
}

void facade::request_post_id()
{
   ss_menu_pub.send(incr(cfg.post_id_key));
   menu_pub_queue.push(request::post_id);
}

void facade::request_user_id()
{
   ss_menu_pub.send(incr(cfg.user_id_key));
   menu_pub_queue.push(request::user_id);
}

void facade::register_user(std::string const& user, std::string const& pwd)
{
   auto const key =  cfg.user_data_prefix_key + user;
   ss_menu_pub.send(hset(key, "password", pwd));
   menu_pub_queue.push(request::register_user);
}

void facade::retrieve_user_data(std::string const& user)
{
   auto const key =  cfg.user_data_prefix_key + user;
   ss_menu_pub.send(hget(key, "password"));
   menu_pub_queue.push(request::user_data);
}

void facade::retrieve_posts(int begin)
{
   log( loglevel::debug, "W{0}/facade::retrieve_posts({1})."
      , worker_id, begin);

   ss_menu_pub.send(zrangebyscore(cfg.posts_key, begin, -1));
   menu_pub_queue.push(request::posts);
}

void facade::disconnect()
{
   ss_menu_sub.close();
   ss_menu_pub.close();
   ss_chat_sub.close();
   ss_chat_pub.close();
}

}

