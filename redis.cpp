#include "redis.hpp"

namespace rt::redis
{

facade::facade(config const& cf, net::io_context& ioc)
: menu_sub(cf.sessions, ioc, request::unsolicited_publish)
, msg_not(cf.sessions, ioc, request::unsolicited_key_not)
, pub_session(cf.sessions, ioc, request::unknown)
, nms(cf.nms)
{
   auto const handler = [this]()
   {
      std::initializer_list<std::string const> const param =
         {nms.menu_channel};

      auto cmd_str = resp_assemble( "SUBSCRIBE"
                                  , std::begin(param)
                                  , std::end(param));

      menu_sub.send({request::subscribe, std::move(cmd_str), ""});
   };

   menu_sub.set_on_conn_handler(handler);

   worker_handler = [](auto const& data, auto const& req) {};
}

void facade::async_retrieve_menu()
{
   std::initializer_list<std::string const> const param =
      {nms.menu_key};

   auto cmd_str = resp_assemble( "GET"
                               , std::begin(param)
                               , std::end(param));

   pub_session.send({request::get_menu, std::move(cmd_str), ""});
}

void facade::set_on_msg_handler(msg_handler_type h)
{
   worker_handler = h;

   auto const sub_handler = [h]( auto const& ec
                               , auto const& data
                               , auto const& req)
   {
      if (ec) {
         // TODO: Should we handle this here or pass to the mgr?
         fail(ec,"sub_handler");
         return;
      }

      assert(std::size(data) == 3);

      // It looks like when subscribing to a redis channel, the
      // confimation is returned twice!?
      if (data.front() != "message")
         return;

      //assert(data[1] == nms.menu_channel);
      h({std::move(data.back())}, req);
   };

   menu_sub.set_msg_handler(sub_handler);

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

void facade::run()
{
   menu_sub.run();
   msg_not.run();
   pub_session.run();
}

void
facade::pub_handler( boost::system::error_code const& ec
                   , std::vector<std::string> const& data
                   , req_data const& req)
{
   // TODO: Should we handle this here or pass to the mgr?
   if (ec) {
      fail(ec,"pub_handler");
      return;
   }

   worker_handler({std::move(data.back())}, req);
}

void
facade::msg_not_handler( boost::system::error_code const& ec
                       , std::vector<std::string> const& data
                       , req_data const& req)
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

   pub_session.send({request::user_msgs, std::move(cmd_str), user_id });
}

void facade::subscribe_to_chat_msgs(std::string const& id)
{
   std::initializer_list<std::string const> const param =
      {nms.notify_prefix + id};

   auto cmd_str = resp_assemble( "SUBSCRIBE"
                               , std::begin(param)
                               , std::end(param));

   msg_not.send({request::subscribe, std::move(cmd_str), ""});
}

void facade::unsubscribe_to_chat_msgs(std::string const& id)
{
   std::initializer_list<std::string const> const param =
      {nms.notify_prefix + id};

   auto cmd_str = resp_assemble( "UNSUBSCRIBE"
                               , std::begin(param)
                               , std::end(param));

   msg_not.send({request::unsubscribe, std::move(cmd_str), ""});
}

void facade::publish_menu_msg(std::string msg)
{
   std::initializer_list<std::string const> const param =
      {nms.menu_channel, std::move(msg)};

   auto cmd_str = resp_assemble( "PUBLISH"
                               , std::begin(param)
                               , std::end(param));

   pub_session.send({request::publish, std::move(cmd_str), ""});
}

void facade::disconnect()
{
   std::cout << "Shuting down redis group subscribe session ..."
             << std::endl;

   menu_sub.close();

   std::cout << "Shuting down redis publish session ..."
             << std::endl;

   pub_session.close();

   std::cout << "Shuting down redis user msg subscribe session ..."
             << std::endl;

   msg_not.close();
}

}

