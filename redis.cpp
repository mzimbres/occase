#include "redis.hpp"

namespace rt::redis
{

facade::facade(config const& cf, net::io_context& ioc)
: menu_sub(cf.sessions, ioc, request::unsolicited_publish)
, msg_not(cf.sessions, ioc, request::unsolicited_key_not)
, pub(cf.sessions, ioc, request::unknown)
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
}

void facade::async_retrieve_menu()
{
   std::initializer_list<std::string const> const param =
      {nms.menu_key};

   auto cmd_str = resp_assemble( "GET"
                               , std::begin(param)
                               , std::end(param));

   pub.send({request::get_menu, std::move(cmd_str), ""});
}

void facade::set_on_msg_handler(msg_handler_type h)
{
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
      h(ec, {std::move(data.back())}, req);
   };

   menu_sub.set_msg_handler(sub_handler);
   pub.set_msg_handler(h);

   // We do not have to pass keyspace notifications to the server
   // menager. It just flags we should retrieve the message.
   auto const key_not_handler = [this]( auto const& ec
                                      , auto const& data
                                      , auto const& req)
   {
      if (data.back() == "rpush") {
         assert(data.front() == "message");
         assert(std::size(data) == 3);
         auto const n = data[1].rfind(":");
         assert(n != std::string::npos);
         async_retrieve_msgs(data[1].substr(n + 1));
      }
   };

   msg_not.set_msg_handler(key_not_handler);
}

void facade::run()
{
   menu_sub.run();
   msg_not.run();
   pub.run();
}

void facade::async_retrieve_msgs(std::string const& user_id)
{
   std::initializer_list<std::string const> const param =
      {nms.msg_prefix + user_id};

   auto cmd_str = resp_assemble( "LPOP"
                               , std::begin(param)
                               , std::end(param));

   pub.send({request::retrieve_msgs, std::move(cmd_str), user_id });
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

   pub.send({request::publish, std::move(cmd_str), ""});
}

void facade::disconnect()
{
   std::cout << "Shuting down redis group subscribe session ..."
             << std::endl;

   menu_sub.close();

   std::cout << "Shuting down redis publish session ..."
             << std::endl;

   pub.close();

   std::cout << "Shuting down redis user msg subscribe session ..."
             << std::endl;

   msg_not.close();
}

}

