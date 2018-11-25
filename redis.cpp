#include "redis.hpp"

namespace rt::redis
{

void facade::async_retrieve_menu()
{
   req_data r
   { request::get_menu
   , gen_resp_cmd(command::get, {nms.menu_key})
   , ""
   };

   pub.send(std::move(r));
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

void facade::subscribe_to_menu_msgs()
{
   req_data r
   { request::subscribe
   , gen_resp_cmd(command::subscribe, {nms.menu_channel})
   , ""
   };

   menu_sub.send(std::move(r));
}

void facade::subscribe_to_chat_msgs(std::string const& id)
{
   req_data r
   { request::subscribe
   , gen_resp_cmd( command::subscribe
                 , {nms.notify_prefix + id})
   , ""
   };

   key_sub.send(std::move(r));
}

void facade::unsubscribe_to_chat_msgs(std::string const& id)
{
   req_data r
   { request::unsubscribe
   , gen_resp_cmd(command::unsubscribe, { nms.notify_prefix + id})
   , ""
   };

   key_sub.send(std::move(r));
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
   , gen_resp_cmd(command::publish, {nms.menu_channel, msg})
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

   key_sub.close();
}

}

