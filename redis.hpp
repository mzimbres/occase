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

struct facade {
   namespaces nms;

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
};

}

