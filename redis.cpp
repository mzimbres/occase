#include "redis.hpp"

namespace rt::redis
{

facade::facade(config const& cf, net::io_context& ioc)
: ss_menu_sub(cf.ss_cf, ioc)
, ss_menu_pub(cf.ss_cf, ioc)
, ss_user_msg_sub(cf.ss_cf, ioc)
, ss_user_msg_retr(cf.ss_cf, ioc)
, nms(cf.nms)
, worker_handler([](auto const& data, auto const& req) {})
{
   // Sets on connection handlers.
   auto on_conn_a = [this]() { on_menu_sub_conn(); };
   auto on_conn_b = [this]() { on_menu_pub_conn(); };
   auto on_conn_c = [this]() { on_user_msg_sub_conn(); };
   auto on_conn_d = [this]() { on_user_msg_retr_conn(); };

   ss_menu_sub.set_on_conn_handler(on_conn_a);
   ss_menu_pub.set_on_conn_handler(on_conn_b);
   ss_user_msg_sub.set_on_conn_handler(on_conn_c);
   ss_user_msg_retr.set_on_conn_handler(on_conn_d);

   // Sets on msg handlers.
   auto on_msg_a = [this](auto const& ec, auto const& data)
      { on_menu_sub_msg(ec, data); };
   auto on_msg_b = [this](auto const& ec, auto const& data)
      { on_menu_pub_msg(ec, data); };
   auto on_msg_c = [this](auto const& ec, auto const& data)
      { on_user_msg_sub_msg(ec, data); };
   auto on_msg_d = [this](auto const& ec, auto const& data)
      { on_user_msg_retr_msg(ec, data); };

   ss_menu_sub.set_msg_handler(on_msg_a);
   ss_menu_pub.set_msg_handler(on_msg_b);
   ss_user_msg_sub.set_msg_handler(on_msg_c);
   ss_user_msg_retr.set_msg_handler(on_msg_d);
}

void facade::on_menu_sub_conn()
{
   std::clog << "on_menu_sub_conn: connected" << std::endl;

   auto param = {nms.menu_channel};

   auto cmd_str = resp_assemble( "SUBSCRIBE"
                               , std::begin(param)
                               , std::end(param));

   ss_menu_sub.send(std::move(cmd_str));
}

void facade::on_menu_pub_conn()
{
   std::clog << "on_menu_pub_conn: connected" << std::endl;
}

void facade::on_user_msg_sub_conn()
{
   std::clog << "on_user_msg_sub_conn: connected" << std::endl;
}

void facade::on_user_msg_retr_conn()
{
   std::clog << "on_user_msg_retr_conn: connected" << std::endl;
}

void facade::async_retrieve_menu()
{
   auto param = {nms.menu_key};

   auto cmd_str = resp_assemble( "GET"
                               , std::begin(param)
                               , std::end(param));

   ss_menu_pub.send(std::move(cmd_str));
   pub_ev_queue.push({request::get_menu, {}});
}

void facade::on_menu_sub_msg( boost::system::error_code const& ec
                            , std::vector<std::string> const& data)
{
   if (ec) {
      fail(ec,"on_menu_sub_msg");
      return; // TODO: Add error handling.
   }

   assert(!std::empty(data));

   // It looks like when subscribing to a redis channel, the
   // confimation is returned twice!?
   if (data.front() != "message")
      return;

   assert(std::size(data) == 3);

   //assert(data[1] == nms.menu_channel);
   // TODO: Take data by value to be able to move items here.
   worker_handler( {std::move(data.back())}
                 , {request::unsolicited_publish, {}});
}

void facade::run()
{
   ss_menu_sub.run();
   ss_menu_pub.run();
   ss_user_msg_sub.run();
   ss_user_msg_retr.run();
}

void
facade::on_menu_pub_msg( boost::system::error_code const& ec
                       , std::vector<std::string> const& data)
{
   if (ec) {
      fail(ec,"on_menu_pub_msg");
      return;
   }

   assert(!std::empty(data));

   // This session is not subscribed to any unsolicited message.
   assert(!std::empty(pub_ev_queue));

   if (pub_ev_queue.front().req == request::ignore) {
      pub_ev_queue.pop();
      return;
   }

   worker_handler({std::move(data.back())}, pub_ev_queue.front());
   pub_ev_queue.pop();
}

void
facade::on_user_msg_retr_msg( boost::system::error_code const& ec
                            , std::vector<std::string> const& data)
{
   if (ec) {
      fail(ec,"on_user_msg_retr_msg");
      return;
   }

   assert(!std::empty(data));

   // This session is not subscribed to any unsolicited message.
   assert(!std::empty(user_msg_queue));

   if (user_msg_queue.front().req == request::get_user_msg) {
      req_item const item { request::unsol_user_msgs
                          , std::move(user_msg_queue.front().user_id)};
      worker_handler({std::move(data.back())}, item);
   }

   user_msg_queue.pop();
}

void
facade::on_user_msg_sub_msg( boost::system::error_code const& ec
                           , std::vector<std::string> const& data)
{
   if (ec) {
      fail(ec,"on_user_msg_sub_msg");
      return;
   }

   if (data.back() != "rpush")
      return;

   assert(data.front() == "message");
   assert(std::size(data) == 3);

   // TODO: Use the prefix size to get the usbstring instead of
   // searching.
   auto const n = data[1].rfind(":");
   assert(n != std::string::npos);
   auto user_id = data[1].substr(n + 1);
   auto param = {nms.msg_prefix + user_id};

   auto cmd_str = resp_assemble( "LPOP"
                               , std::begin(param)
                               , std::end(param));

   ss_user_msg_retr.send(std::move(cmd_str));
   user_msg_queue.push({request::get_user_msg, std::move(user_id)});
}

void facade::subscribe_to_chat_msgs(std::string const& id)
{
   auto param = {nms.notify_prefix + id};

   auto cmd_str = resp_assemble( "SUBSCRIBE"
                               , std::begin(param)
                               , std::end(param));

   ss_user_msg_sub.send(std::move(cmd_str));
}

void facade::unsubscribe_to_chat_msgs(std::string const& id)
{
   auto param = {nms.notify_prefix + id};

   auto cmd_str = resp_assemble( "UNSUBSCRIBE"
                               , std::begin(param)
                               , std::end(param));

   ss_user_msg_sub.send(std::move(cmd_str));
}

void facade::publish_menu_msg(std::string msg)
{
   // Using pipeline and transactions toguether.
   //auto par0 = {};

   //auto cmd = resp_assemble( "MULTI"
   //                        , std::begin(par0)
   //                        , std::end(par0));

   auto par2 = {nms.menu_channel, msg};

   auto cmd = resp_assemble( "PUBLISH"
                       , std::begin(par2)
                       , std::end(par2));

   //cmd += resp_assemble( "EXEC"
   //                    , std::begin(par0)
   //                    , std::end(par0));

   ss_menu_pub.send(std::move(cmd));
   //pub_ev_queue.push({request::ignore, {}});
   //pub_ev_queue.push({request::ignore, {}});
   pub_ev_queue.push({request::publish, {}});
}

void facade::request_pub_id()
{
   auto par1 = {"pub_counter"};

   auto cmd = resp_assemble( "INCR"
                           , std::begin(par1)
                           , std::end(par1));

   ss_menu_pub.send(std::move(cmd));
   pub_ev_queue.push({request::pub_counter, {}});
}

void facade::disconnect()
{
   std::clog << "ss_menu_sub: Closing." << std::endl;
   ss_menu_sub.close();

   std::clog << "ss_menu_sub: Closing." << std::endl;
   ss_menu_pub.close();

   std::clog << "ss_menu_pub: Closing." << std::endl;
   ss_user_msg_sub.close();

   std::clog << "ss_user_msg_retr: Closing." << std::endl;
   ss_user_msg_retr.close();
}

}

