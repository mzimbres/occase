#include "redis.hpp"

#include <fmt/format.h>

#include "utils.hpp"

namespace rt::redis
{

facade::facade(config const& cfg, net::io_context& ioc, int wid)
: worker_id {wid}
, ss_menu_sub {cfg.ss_cfg, ioc, fmt::format("W{0}/db-menu_sub", wid)}
, ss_menu_pub {cfg.ss_cfg, ioc, fmt::format("W{0}/db-menu_pub", wid)}
, ss_user_sub {cfg.ss_cfg, ioc, fmt::format("W{0}/db-user_sub", wid)}
, ss_user_pub {cfg.ss_cfg, ioc, fmt::format("W{0}/db-user_pub", wid)}
, cfg(cfg.cfg)
, worker_handler([](auto const& data, auto const& req) {})
{
   // Sets on connection handlers.
   auto on_conn_a = [this]() { on_menu_sub_conn(); };
   auto on_conn_b = [this]() { on_menu_pub_conn(); };
   auto on_conn_c = [this]() { on_user_sub_conn(); };
   auto on_conn_d = [this]() { on_user_pub_conn(); };

   ss_menu_sub.set_on_conn_handler(on_conn_a);
   ss_menu_pub.set_on_conn_handler(on_conn_b);
   ss_user_sub.set_on_conn_handler(on_conn_c);
   ss_user_pub.set_on_conn_handler(on_conn_d);

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
   ss_user_sub.set_msg_handler(on_msg_c);
   ss_user_pub.set_msg_handler(on_msg_d);
}

void facade::on_menu_sub_conn()
{
   auto const* fmt = "W{0}/facade::on_menu_sub_conn: connected.";
   log(fmt::format(fmt, worker_id), loglevel::debug);
   ss_menu_sub.send(subscribe(cfg.menu_channel));
}

void facade::on_menu_pub_conn()
{
   auto const* fmt = "W{0}/facade::on_menu_pub_conn: connected.";
   log(fmt::format(fmt, worker_id), loglevel::debug);
   worker_handler({}, {request::menu_connect, {}});
}

void facade::on_user_sub_conn()
{
   auto const* fmt = "W{0}/facade::on_user_sub: connected.";
   log(fmt::format(fmt, worker_id), loglevel::debug);
}

void facade::on_user_pub_conn()
{
   auto const* fmt = "W{0}/facade::on_user_pub: connected.";
   log(fmt::format(fmt, worker_id), loglevel::debug);
}

void facade::async_retrieve_menu()
{
   ss_menu_pub.send(get(cfg.menu_key));
   menu_pub_queue.push(request::get_menu);
}

void facade::on_menu_sub( boost::system::error_code const& ec
                        , std::vector<std::string> data)
{
   if (ec) {
      auto const* fmt = "W{0}/facade::on_menu_sub: {1}.";
      log(fmt::format(fmt, worker_id, ec.message()), loglevel::debug);
      return;
   }

   assert(!std::empty(data));

   if (data.front() != "message")
      return;

   assert(std::size(data) == 3);
   assert(data[1] == cfg.menu_channel);

   std::swap(data.front(), data.back());
   data.resize(1);
   worker_handler(data , {request::unsol_publish, {}});
}

void facade::run()
{
   ss_menu_sub.run();
   ss_menu_pub.run();
   ss_user_sub.run();
   ss_user_pub.run();
}

void
facade::on_menu_pub( boost::system::error_code const& ec
                   , std::vector<std::string> data)
{
   if (ec) {
      auto const* fmt = "W{0}/facade::on_menu_pub: {1}.";
      log(fmt::format(fmt, worker_id, ec.message()), loglevel::debug);
      return;
   }

   if (menu_pub_queue.front() != request::menu_msgs) {
      assert(!std::empty(data));
   }

   // This session is not subscribed to any unsolicited message.
   assert(!std::empty(menu_pub_queue));

   if (menu_pub_queue.front() == request::ignore) {
      menu_pub_queue.pop();
      return;
   }

   worker_handler(data, {menu_pub_queue.front(), {}});
   menu_pub_queue.pop();
}

void
facade::on_user_pub( boost::system::error_code const& ec
                   , std::vector<std::string> data)
{
   if (ec) {
      auto const* fmt = "W{0}/facade::on_user_sub: {1}.";
      log(fmt::format(fmt, worker_id, ec.message()), loglevel::debug);
      return;
   }

   if (std::empty(data)) {
      user_pub_queue.pop();
      return;
   }

   // This session is not subscribed to any unsolicited message.
   assert(!std::empty(user_pub_queue));

   if (user_pub_queue.front().req == request::get_user_msg) {
      req_item const item { request::unsol_user_msgs
                          , std::move(user_pub_queue.front().user_id)};

      worker_handler(data, item);
   }

   user_pub_queue.pop();
}

void
facade::on_user_sub( boost::system::error_code const& ec
                   , std::vector<std::string> data)
{
   if (ec) {
      auto const* fmt = "W{0}/facade::on_user_sub: {1}.";
      log(fmt::format(fmt, worker_id, ec.message()), loglevel::debug);
      return;
   }

   if (data.back() != "lpush")
      return;

   assert(data.front() == "message");
   assert(std::size(data) == 3);

   auto const pos = std::size(cfg.user_notify_prefix);
   auto user_id = data[1].substr(pos);
   assert(!std::empty(user_id));
   retrieve_user_msgs(user_id);
}

void facade::retrieve_user_msgs(std::string const& user_id)
{
   auto const key = cfg.msg_prefix + user_id;
   auto cmd = multi()
            + lrange(key, 0, -1)
            + del(key)
            + exec();

   ss_user_pub.send(std::move(cmd));
   user_pub_queue.push({request::ignore, {}});
   user_pub_queue.push({request::ignore, {}});
   user_pub_queue.push({request::ignore, {}});
   user_pub_queue.push({request::get_user_msg, user_id});
   user_pub_queue.push({request::ignore, {}});
}

void facade::sub_to_user_msgs(std::string const& id)
{
   ss_user_sub.send(subscribe(cfg.user_notify_prefix + id));
}

void facade::unsub_to_user_msgs(std::string const& id)
{
   ss_user_sub.send(unsubscribe(cfg.user_notify_prefix + id));
}

void facade::pub_menu_msg(std::string const& msg, int id)
{
   auto cmd = multi()
            + zadd(cfg.menu_msgs_key, id, msg)
            + publish(cfg.menu_channel, msg)
            + exec();

   ss_menu_pub.send(std::move(cmd));
   menu_pub_queue.push(request::ignore);
   menu_pub_queue.push(request::ignore);
   menu_pub_queue.push(request::ignore);
   menu_pub_queue.push(request::publish);
}

void facade::request_pub_id()
{
   ss_menu_pub.send(incr(cfg.menu_msgs_counter_key));
   menu_pub_queue.push(request::pub_counter);
}

void facade::retrieve_menu_msgs(int begin)
{
   ss_menu_pub.send(zrangebyscore(cfg.menu_msgs_key, begin, -1));
   menu_pub_queue.push(request::menu_msgs);
}

void facade::disconnect()
{
   ss_menu_sub.close();
   ss_menu_pub.close();
   ss_user_sub.close();
   ss_user_pub.close();
}

}

