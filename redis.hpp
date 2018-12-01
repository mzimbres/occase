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
   // The session used to subscribe to menu messages.
   session menu_sub;

   // The session used for keyspace notifications e.g. when the user
   // receives a message.
   session msg_not;

   // Redis session to send general commands.
   session pub;

   namespaces nms;

public:
   using msg_handler_type = session::msg_handler_type;

   facade(config const& cf, net::io_context& ioc)
   : menu_sub(cf.sessions, ioc)
   , msg_not(cf.sessions, ioc)
   , pub(cf.sessions, ioc)
   , nms(cf.nms)
   { }

   void set_menu_msg_handler(msg_handler_type h)
   { menu_sub.set_msg_handler(h); }

   void set_msg_not_handler(msg_handler_type h)
   { msg_not.set_msg_handler(h); }

   void set_cmd_handler(msg_handler_type h)
   { pub.set_msg_handler(h); }

   void run()
   {
      menu_sub.run();
      msg_not.run();
      pub.run();
   }

   void async_retrieve_menu();
   void async_retrieve_msgs(std::string const& user_id);
   void subscribe_to_menu_msgs();
   void subscribe_to_chat_msgs(std::string const& id);
   void unsubscribe_to_chat_msgs(std::string const& id);
   void async_store_chat_msg(std::string id, std::string msg);
   void publish_menu_msg(std::string msg);
   void disconnect();
};

}

