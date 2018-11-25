#pragma once

#include <string>

#include "redis_session.hpp"

namespace rt::redis
{

struct namespaces {
   std::string menu_channel;
   std::string menu_key;

   // The prefix added to all keys that store user messages. The final
   // key will be a composition of this prefix and the user id
   // separate by a ":".
   std::string msg_prefix;
   std::string notify_prefix {"__keyspace@0__:"};
};

struct config {
   session_cf sessions;
   namespaces nms;
};

class facade {
private:
   namespaces nms;
public:

   // The session used to subscribe to menu messages.
   session menu_sub;

   // The session used for keyspace notifications e.g. when the user
   // receives a message.
   session key_sub;

   // Redis session to send general commands.
   session pub;

   facade(config const& cf, net::io_context& ioc)
   : menu_sub(cf.sessions, ioc)
   , key_sub(cf.sessions, ioc)
   , pub(cf.sessions, ioc)
   , nms(cf.nms)
   {
   }

   void async_retrieve_menu()
   {
      req_data r
      { request::get_menu
      , gen_resp_cmd(command::get, {nms.menu_key})
      , ""
      };

      pub.send(std::move(r));
   }

   void async_retrieve_msgs(std::string const& user_id)
   {
      auto const key = nms.msg_prefix + user_id;
      req_data r
      { request::retrieve_msgs
      , gen_resp_cmd(command::lpop, {key})
      , user_id
      };

      pub.send(r);
   }

   void subscribe_to_menu_msgs()
   {
      req_data r
      { request::subscribe
      , gen_resp_cmd(command::subscribe, {nms.menu_channel})
      , ""
      };

      menu_sub.send(std::move(r));
   }

   void subscribe_to_chat_msgs(std::string const& id)
   {
      req_data r
      { request::subscribe
      , gen_resp_cmd( command::subscribe
                    , {nms.notify_prefix + id})
      , ""
      };

      key_sub.send(std::move(r));
   }

   void unsubscribe_to_chat_msgs(std::string const& id)
   {
      req_data r
      { request::unsubscribe
      , gen_resp_cmd(command::unsubscribe, { nms.notify_prefix + id})
      , ""
      };

      key_sub.send(std::move(r));
   }

   void async_store_chat_msg(std::string id, std::string msg)
   {
      req_data r
      { request::store_msg
      , gen_resp_cmd(command::rpush, {nms.msg_prefix + std::move(id), msg})
      , ""
      };

      pub.send(std::move(r));
   }

   void publish_menu_msg(std::string msg)
   {
      req_data r
      { request::publish
      , gen_resp_cmd(command::publish, {nms.menu_channel, msg})
      , ""
      };

      pub.send(std::move(r));
   }

   void disconnect()
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
};

}

