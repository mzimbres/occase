#include "redis.hpp"

namespace rt::redis
{

facade::facade(config const& cf, net::io_context& ioc)
: ss_menu_sub(cf.sessions, ioc)
, ss_menu_pub(cf.sessions, ioc)
, ss_user_msg_sub(cf.sessions, ioc)
, ss_user_msg_retriever(cf.sessions, ioc)
, nms(cf.nms)
{
   auto const handler = [this]()
   {
      std::initializer_list<std::string const> const param =
         {nms.menu_channel};

      auto cmd_str = resp_assemble( "SUBSCRIBE"
                                  , std::begin(param)
                                  , std::end(param));

      ss_menu_sub.send(std::move(cmd_str));
   };

   ss_menu_sub.set_on_conn_handler(handler);

   worker_handler = [](auto const& data, auto const& req) {};

   auto const a = [this](auto const& ec, auto const& data)
      { menu_sub_handler(ec, data); };

   ss_menu_sub.set_msg_handler(a);

   auto const b = [this](auto const& ec, auto const& data)
      { menu_pub_handler(ec, data); };

   ss_menu_pub.set_msg_handler(b);

   auto const c = [this](auto const& ec, auto const& data)
      { user_msg_sub_handler(ec, data); };

   ss_user_msg_sub.set_msg_handler(c);
}

void facade::async_retrieve_menu()
{
   std::initializer_list<std::string const> const param =
      {nms.menu_key};

   auto cmd_str = resp_assemble( "GET"
                               , std::begin(param)
                               , std::end(param));

   ss_menu_pub.send(std::move(cmd_str));
   pub_ev_queue.push({request::get_menu, {}});
}

void facade::menu_sub_handler( boost::system::error_code const& ec
                        , std::vector<std::string> const& data)
{
   if (ec) {
      fail(ec,"menu_sub_handler");
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
   ss_user_msg_sub.run();
   ss_menu_pub.run();
}

void
facade::menu_pub_handler( boost::system::error_code const& ec
                        , std::vector<std::string> const& data)
{
   if (ec) { // TODO: Should we handle this here or pass to the mgr?
      fail(ec,"menu_pub_handler");
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
facade::user_msg_sub_handler( boost::system::error_code const& ec
                            , std::vector<std::string> const& data)
{
   if (ec) {
      fail(ec,"user_msg_sub_handler");
      return;
   }

   if (data.back() != "rpush")
      return;

   assert(data.front() == "message");
   assert(std::size(data) == 3);
   auto const n = data[1].rfind(":");
   assert(n != std::string::npos);
   auto const user_id = data[1].substr(n + 1);
   std::initializer_list<std::string> const param =
      {nms.msg_prefix + user_id};

   auto cmd_str = resp_assemble( "LPOP"
                               , std::begin(param)
                               , std::end(param));

   ss_menu_pub.send(std::move(cmd_str));
   pub_ev_queue.push({request::unsol_user_msgs, user_id});
}

void facade::subscribe_to_chat_msgs(std::string const& id)
{
   std::initializer_list<std::string const> const param =
      {nms.notify_prefix + id};

   auto cmd_str = resp_assemble( "SUBSCRIBE"
                               , std::begin(param)
                               , std::end(param));

   ss_user_msg_sub.send(std::move(cmd_str));
}

void facade::unsubscribe_to_chat_msgs(std::string const& id)
{
   std::initializer_list<std::string const> const param =
      {nms.notify_prefix + id};

   auto cmd_str = resp_assemble( "UNSUBSCRIBE"
                               , std::begin(param)
                               , std::end(param));

   ss_user_msg_sub.send(std::move(cmd_str));
}

void facade::publish_menu_msg(std::string msg)
{
   // Using pipeline and transactions toguether.
   std::initializer_list<std::string> par0 = {};

   //auto cmd = resp_assemble( "MULTI"
   //                        , std::begin(par0)
   //                        , std::end(par0));

   std::initializer_list<std::string> par2 = {nms.menu_channel, msg};

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
   std::initializer_list<std::string> par1 = {"pub_counter"};

   auto cmd = resp_assemble( "INCR"
                           , std::begin(par1)
                           , std::end(par1));

   ss_menu_pub.send(std::move(cmd));
   pub_ev_queue.push({request::pub_counter, {}});
}

void facade::disconnect()
{
   std::cout << "Shuting down redis group subscribe session ..."
             << std::endl;

   ss_menu_sub.close();

   std::cout << "Shuting down redis publish session ..."
             << std::endl;

   ss_menu_pub.close();

   std::cout << "Shuting down redis user msg subscribe session ..."
             << std::endl;

   ss_user_msg_sub.close();
}

}

