#include "redis.hpp"

namespace rt::redis
{

facade::facade(config const& cf, net::io_context& ioc)
: menu_sub_session(cf.sessions, ioc)
, msg_not(cf.sessions, ioc)
, pub_session(cf.sessions, ioc)
, nms(cf.nms)
{
   auto const handler = [this]()
   {
      std::initializer_list<std::string const> const param =
         {nms.menu_channel};

      auto cmd_str = resp_assemble( "SUBSCRIBE"
                                  , std::begin(param)
                                  , std::end(param));

      menu_sub_session.send(std::move(cmd_str));
   };

   menu_sub_session.set_on_conn_handler(handler);

   worker_handler = [](auto const& data, auto const& req) {};

   auto const subh = [this]( auto const& ec
                           , auto const& data
                           , auto const& req)
   { sub_handler(ec, data, req); };

   menu_sub_session.set_msg_handler(subh);

   auto const pubh = [this]( auto const& ec
                           , auto const& data
                           , auto const& req)
   { pub_handler(ec, data, req); };

   pub_session.set_msg_handler(pubh);

   auto const key_not_handler = [this]( auto const& ec
                                      , auto const& data
                                      , auto const& req)
   { msg_not_handler(ec, data, req); };

   msg_not.set_msg_handler(key_not_handler);
}

void facade::async_retrieve_menu()
{
   std::initializer_list<std::string const> const param =
      {nms.menu_key};

   auto cmd_str = resp_assemble( "GET"
                               , std::begin(param)
                               , std::end(param));

   pub_session.send(std::move(cmd_str));
   pub_ev_queue.push({request::get_menu, {}});
}

void facade::sub_handler( boost::system::error_code const& ec
                        , std::vector<std::string> const& data
                        , std::string const& req)
{
   if (ec) {
      fail(ec,"sub_handler");
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
   menu_sub_session.run();
   msg_not.run();
   pub_session.run();
}

void
facade::pub_handler( boost::system::error_code const& ec
                   , std::vector<std::string> const& data
                   , std::string const& req)
{
   // TODO: Should we handle this here or pass to the mgr?
   if (ec) {
      fail(ec,"pub_handler");
      return;
   }

   assert(!std::empty(data));

   // This session is not subscribed to any unsolicited message.
   assert(!std::empty(pub_ev_queue));

   if (pub_ev_queue.front().req == request::ignore) {
      pub_ev_queue.pop();
      return;
   }

   if (pub_ev_queue.front().req == request::publish) {
      pub_ev_queue.pop();
      return;
   }

   worker_handler({std::move(data.back())}, pub_ev_queue.front());
   pub_ev_queue.pop();
}

void
facade::msg_not_handler( boost::system::error_code const& ec
                       , std::vector<std::string> const& data
                       , std::string const& req)
{
   // TODO: Handle ec.
   if (ec) {
      fail(ec,"pub_handler");
      return;
   }

   if (data.back() != "rpush")
      return;

   assert(data.front() == "message");
   assert(std::size(data) == 3);
   auto const n = data[1].rfind(":");
   assert(n != std::string::npos);
   std::string const user_id = data[1].substr(n + 1);
   std::initializer_list<std::string> const param =
      {nms.msg_prefix + user_id};

   auto cmd_str = resp_assemble( "LPOP"
                               , std::begin(param)
                               , std::end(param));

   pub_session.send(std::move(cmd_str));
   pub_ev_queue.push({request::unsol_user_msgs, user_id});
}

void facade::subscribe_to_chat_msgs(std::string const& id)
{
   std::initializer_list<std::string const> const param =
      {nms.notify_prefix + id};

   auto cmd_str = resp_assemble( "SUBSCRIBE"
                               , std::begin(param)
                               , std::end(param));

   msg_not.send(std::move(cmd_str));
}

void facade::unsubscribe_to_chat_msgs(std::string const& id)
{
   std::initializer_list<std::string const> const param =
      {nms.notify_prefix + id};

   auto cmd_str = resp_assemble( "UNSUBSCRIBE"
                               , std::begin(param)
                               , std::end(param));

   msg_not.send(std::move(cmd_str));
}

void facade::publish_menu_msg(std::string msg)
{
   // Using pipeline and transactions toguether.
   std::initializer_list<std::string> par0 = {};

   auto cmd = resp_assemble( "MULTI"
                           , std::begin(par0)
                           , std::end(par0));

   std::initializer_list<std::string> par1 = {"pub_counter"};

   cmd += resp_assemble( "INCR"
                       , std::begin(par1)
                       , std::end(par1));

   std::initializer_list<std::string> par2 = {nms.menu_channel, msg};

   cmd += resp_assemble( "PUBLISH"
                       , std::begin(par2)
                       , std::end(par2));

   cmd += resp_assemble( "EXEC"
                       , std::begin(par0)
                       , std::end(par0));

   pub_session.send(std::move(cmd));
   pub_ev_queue.push({request::ignore, {}});
   pub_ev_queue.push({request::ignore, {}});
   pub_ev_queue.push({request::ignore, {}});
   pub_ev_queue.push({request::publish, {}});
}

void facade::disconnect()
{
   std::cout << "Shuting down redis group subscribe session ..."
             << std::endl;

   menu_sub_session.close();

   std::cout << "Shuting down redis publish session ..."
             << std::endl;

   pub_session.close();

   std::cout << "Shuting down redis user msg subscribe session ..."
             << std::endl;

   msg_not.close();
}

}

