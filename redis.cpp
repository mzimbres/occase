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
      menu_sub.send({ request::subscribe
                    , gen_resp_cmd(command::subscribe, {nms.menu_channel})
                    , ""
                    });
   };

   menu_sub.set_on_conn_handler(handler);
}

void facade::async_retrieve_menu()
{
   req_data r
   { request::get_menu
   , gen_resp_cmd(command::get, {nms.menu_key})
   , ""
   };

   pub.send(std::move(r));
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
   msg_not.set_msg_handler(h);
   pub.set_msg_handler(h);
}

void facade::run()
{
   menu_sub.run();
   msg_not.run();
   pub.run();
}

void facade::async_retrieve_msgs(std::string const& user_id)
{
   auto const key = nms.msg_prefix + user_id;
   req_data r
   { request::retrieve_msgs
   , gen_resp_cmd(command::lpop, {key})
   , user_id
   };

   pub.send(r);
}

void facade::subscribe_to_chat_msgs(std::string const& id)
{
   req_data r
   { request::subscribe
   , gen_resp_cmd( command::subscribe
                 , {nms.notify_prefix + id})
   , ""
   };

   msg_not.send(std::move(r));
}

void facade::unsubscribe_to_chat_msgs(std::string const& id)
{
   req_data r
   { request::unsubscribe
   , gen_resp_cmd(command::unsubscribe, { nms.notify_prefix + id})
   , ""
   };

   msg_not.send(std::move(r));
}

void facade::async_store_chat_msg(std::string id, std::string msg)
{
   req_data r
   { request::store_msg
   , gen_resp_cmd(command::rpush, {nms.msg_prefix + std::move(id), msg})
   , ""
   };

   pub.send(std::move(r));
}

void facade::publish_menu_msg(std::string msg)
{
   req_data r
   { request::publish
   , gen_resp_cmd( command::publish
                 , {nms.menu_channel, msg})
   , ""
   };

   pub.send(std::move(r));
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

