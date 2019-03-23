#include "redis.hpp"

#include <fmt/format.h>

#include "utils.hpp"

namespace rt::redis
{

facade::facade(config const& cf, net::io_context& ioc)
: ss_menu_sub(cf.ss_cf, ioc, "1")
, ss_menu_pub(cf.ss_cf, ioc, "2")
, ss_user_sub(cf.ss_cf, ioc, "3")
, ss_user_pub(cf.ss_cf, ioc, "4")
, cf(cf.cf)
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

   // Sets on msg handlers.
   auto on_msg_a = [this](auto const& ec, auto const& data)
      { on_menu_sub(ec, data); };
   auto on_msg_b = [this](auto const& ec, auto const& data)
      { on_menu_pub(ec, data); };
   auto on_msg_c = [this](auto const& ec, auto const& data)
      { on_user_sub(ec, data); };
   auto on_msg_d = [this](auto const& ec, auto const& data)
      { on_user_pub(ec, data); };

   ss_menu_sub.set_msg_handler(on_msg_a);
   ss_menu_pub.set_msg_handler(on_msg_b);
   ss_user_sub.set_msg_handler(on_msg_c);
   ss_user_pub.set_msg_handler(on_msg_d);
}

void facade::on_menu_sub_conn()
{
   log("on_menu_sub_conn: connected", loglevel::debug);
   ss_menu_sub.send(subscribe(cf.menu_channel));
}

void facade::on_menu_pub_conn()
{
   log("on_menu_pub_conn: connected", loglevel::debug);
   worker_handler({}, {request::menu_connect, {}});
}

void facade::on_user_sub_conn()
{
   log("on_user_sub_conn: connected", loglevel::debug);
}

void facade::on_user_pub_conn()
{
   log("on_user_pub_conn: connected", loglevel::debug);
}

void facade::async_retrieve_menu()
{
   ss_menu_pub.send(get(cf.menu_key));
   menu_pub_queue.push(request::get_menu);
}

void facade::on_menu_sub( boost::system::error_code const& ec
                        , std::vector<std::string> const& data)
{
   if (ec) {
      fail(ec,"on_menu_sub");
      return; // TODO: Add error handling.
   }

   assert(!std::empty(data));

   if (data.front() != "message")
      return;

   assert(std::size(data) == 3);

   //assert(data[1] == cf.menu_channel);
   // TODO: Take data by value to be able to move items here.
   worker_handler( {std::move(data.back())}
                 , {request::unsolicited_publish, {}});
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
                   , std::vector<std::string> const& data)
{
   if (ec) {
      fail(ec,"on_menu_pub");
      return;
   }

   if (menu_pub_queue.front() == request::menu_msgs) {
   } else {
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
                   , std::vector<std::string> const& data)
{
   if (ec) {
      fail(ec,"on_user_pub");
      return;
   }

   assert(!std::empty(data));

   // This session is not subscribed to any unsolicited message.
   assert(!std::empty(user_pub_queue));

   if (user_pub_queue.front().req == request::get_user_msg) {
      //std::cout << "=======> ";
      //for (auto const& o : data)
      //  std::cout << o << " ";

      //std::cout << std::endl;

      req_item const item { request::unsol_user_msgs
                          , std::move(user_pub_queue.front().user_id)};
      worker_handler({std::move(data.back())}, item);
   }

   user_pub_queue.pop();
}

void
facade::on_user_sub( boost::system::error_code const& ec
                   , std::vector<std::string> const& data)
{
   if (ec) {
      fail(ec,"on_user_sub");
      return;
   }

   if (data.back() != "lpush")
      return;

   assert(data.front() == "message");
   assert(std::size(data) == 3);

   auto const pos = std::size(cf.notify_prefix);
   auto user_id = data[1].substr(pos);
   assert(!std::empty(user_id));
   retrieve_user_msgs(user_id);
}

void facade::retrieve_user_msgs(std::string const& user_id)
{
   auto cmd = multi()
            + lpop(cf.msg_prefix + user_id)
            + exec();

   ss_user_pub.send(std::move(cmd));
   user_pub_queue.push({request::ignore, {}});
   user_pub_queue.push({request::ignore, {}});
   user_pub_queue.push({request::get_user_msg, user_id});
}

void facade::sub_to_user_msgs(std::string const& id)
{
   ss_user_sub.send(subscribe(cf.notify_prefix + id));
}

void facade::unsub_to_user_msgs(std::string const& id)
{
   ss_user_sub.send(unsubscribe(cf.notify_prefix + id));
}

void facade::pub_menu_msg(std::string const& msg, int id)
{
   auto cmd = multi()
            + zadd(cf.menu_msgs_key, id, msg)
            + publish(cf.menu_channel, msg)
            + exec();

   ss_menu_pub.send(std::move(cmd));
   menu_pub_queue.push(request::ignore);
   menu_pub_queue.push(request::ignore);
   menu_pub_queue.push(request::ignore);
   menu_pub_queue.push(request::publish);
}

void facade::request_pub_id()
{
   ss_menu_pub.send(incr(cf.menu_msgs_counter_key));
   menu_pub_queue.push(request::pub_counter);
}

void facade::retrieve_menu_msgs(int begin)
{
   ss_menu_pub.send(zrange(cf.menu_msgs_key, begin, -1));
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

