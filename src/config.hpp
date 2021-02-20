#pragma once

#include <string>

namespace config {

struct timeouts {
   std::chrono::seconds handshake;
   std::chrono::seconds idle;

   // The deadline for the number of posts.
   std::chrono::seconds post_interval;

   // Time after which the post is considered expired. Input in
   // seconds.
   std::chrono::seconds post_expiration;
};

struct redis {
   // Redis host.
   std::string host;

   // Redis port.
   std::string port;

   // The name of the channel where a node sends the posts it receives
   // on the api url */post/publish* to other nodes.
   std::string posts_channel_key;

   // The key used to store posts in redis.
   std::string posts_key;

   // The prefix of chat message keys. The complete key will be a
   // composition of this prefix and the user id separate by a ":"
   // e.g. msg:102 where 102 is the user id, acquired on registration.
   std::string chat_msg_prefix;

   // This prefix will be used to form the channel where presence
   // messages sent to the user will be published e.g. prefix:102.  We
   // use channel for presence since it does not have to be persisted.
   std::string presence_channel_prefix = "pc:";

   // Redis keyspace notification prefix. When a key is touched redis
   // sends a notification. This is how a worker gets notified that it
   // has to retrieve messages from the database for this user.
   std::string const notify_prefix {"__keyspace@"};

   // Keyspace notification prefix including the message prefix e.g.
   //
   //    __keyspace@0__:msg
   //
   // It is used to read the user from the notification sent by redis.
   std::string user_notify_prefix;

   // Chat messsage counter. It is used only to count the number of
   // messages sent so far.
   std::string chat_msgs_counter_key;

   // The channel where user FCM tokens should be published.
   std::string token_channel {"tokens"};

   // Expiration time for user message keys. Keys will be deleted on
   // expiration and all chat messages that have not been retrieved
   // are gone.
   int chat_msg_exp_time {3600};

   // The maximum number of chat messages a user is allowed to
   // accumulate on the server (when he is offline).
   int max_offline_chat_msgs {100};
};

struct core {
   // The maximum number of posts that are allowed to be sent to the
   // user on subscribe.
   int max_posts_on_search; 

   // The size of the password sent to the app when it registers.
   int pwd_size; 

   // The port on which the database listens.
   unsigned short db_port;

   // TCP backlog size.
   int max_listen_connections;

   // The key used to generate authenticated filenames that will be
   // user in the image server.
   std::string mms_key;

   // The host name of this db. This is used by the adm interface when the html
   // pages are generated.
   std::string db_host;

   // The host where images are served and posted. It must have the
   // form http://occase.de/
   std::string mms_host;

   // The maximum duration time for the adm http session in seconds.
   int http_session_timeout {30};

   // Value of the header field Access-Control-Allow-Origin
   std::string http_allow_origin {"*"};

   // SSL shutdown timeout in seconds.
   int ssl_shutdown_timeout {30};

   // Server name.
   std::string server_name {"occase-db"};

   // The password required to access the adm html pages.
   std::string adm_pwd;

   // The number of posts users are allowed to post until the deadline below is
   // reached. This value is also configurable via adm api.
   int allowed_posts = 0;

   // Redis config.
   config::redis redis;

   // Some timeouts.
   config::timeouts timeouts;
};

} // config


