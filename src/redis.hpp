#pragma once

#include <string>

#include "redis_session.hpp"

namespace rt::redis
{

enum class request
{ get_menu
, chat_messages
, ignore
, publish
, post_id
, user_id
, get_user_msg
, menu_connect
, menu_msgs
, unsol_publish
};

struct req_item {
   request req;
   std::string user_id;
};

struct db_cfg {
   // The name of the channel where *publish* commands will be sent.
   std::string menu_channel_key;

   // Key in redis holding the menu in json format.
   std::string menu_key;

   // The key used to store menu_msgs in redis.
   std::string menu_msgs_key;

   // The prefix of user message keys. The complete key will be a
   // composition of this prefix and the user id separate by a ":"
   // e.g. msg:mzimbres@gmail.com.
   std::string msg_prefix;

   // Redis keyspace notification prefix. When a key is touched redis
   // sendd us a notification. This is how a worker gets notified that
   // it has to retrieve messages from the database for this user.
   std::string notify_prefix {"__keyspace@"};

   // Keyspace notification prefix including the message prefix e.g.
   //
   //    __keyspace@0__:msg
   //
   // It is used to read the user from the notification sent by redis.
   std::string user_notify_prefix;

   // The key used to store the menu message id. Each post gets an id
   // by increasing the last id by one.
   std::string post_id_key;

   // Chat messsage counter. It is used only to count the number of
   // messages sent so far.
   std::string chat_msgs_counter_key;

   // User registration id counter key. When an app connects to the
   // server for the first time it will get an id.
   std::string user_id_key;

   // Expiration time for user message keys. Keys will be deleted on
   // expiration and all chat messages that have not been retrieved
   // are gone.
   int chat_msg_exp_time {3600};

   // The maximum number of chat messages a user is allowed to
   // accumulate on the server (when he is offline).
   int max_offline_msgs {100};
};

struct config {
   session_cfg ss_cfg;
   db_cfg cfg;
};

// Facade class to manage the communication with redis and hide some
// uninteresting details from the workers.
class facade {
public:
   using msg_handler_type =
      std::function<void (std::vector<std::string>, req_item const&)>;

private:
   // The worker id this facade corresponds to.
   int worker_id;

   db_cfg const cfg;

   // The Session used to subscribe to menu messages. No commands
   // should be posted here (with the exception of subscribe and
   // unsubscribe).
   session ss_menu_sub;

   // The session that deals with the publication of menu commands.
   // On startup it will also be used to retrieve menu messages to
   // load the workers.
   session ss_menu_pub;
   std::queue<request> menu_pub_queue;

   // The session used to subscribe to keyspace notifications e.g.
   // when the user receives a message. Again, no commands should be
   // posted here (with the exception of subscribe and unsubscribe)
   session ss_chat_sub;

   // Session used to store and retrieve user messages whose notification
   // arrived on session ss_chat_sub.
   session ss_chat_pub;
   std::queue<req_item> chat_pub_queue;

   msg_handler_type worker_handler;

   void on_menu_sub( boost::system::error_code const& ec
                   , std::vector<std::string> data);

   void on_menu_pub( boost::system::error_code const& ec
                   , std::vector<std::string> data);

   void on_user_sub( boost::system::error_code const& ec
                   , std::vector<std::string> data);

   void on_user_pub( boost::system::error_code const& ec
                   , std::vector<std::string> data);

   void on_menu_sub_conn();
   void on_menu_pub_conn();
   void on_chat_sub_conn();
   void on_chat_pub_conn();

public:
   facade(config const& cfg, net::io_context& ioc, int wid);

   // Incomming message will complete on this handler with
   // one of the possible codes defined in redis::request.
   void set_on_msg_handler(msg_handler_type h)
      { worker_handler = h; }

   void run();

   // Retrieves the menu asynchronously. The callback will complete
   // with redis::request::get_menu.
   void retrieve_menu();

   // Instructs redis to notify us upon any change to the given
   // user_id. Once a notification arrives the server proceeds with
   // the retrieval of the message, which may be more than one by the
   // time we get to it. User messages will complete on workers
   // callback with
   // 
   //    redis::request::chat_messages
   //
   void subscribe_to_chat_msgs(std::string const& user_id);

   // Usubscribe to the notifications to the key. On completion it
   // passes no event to the worker.
   void unsubscribe_to_chat_msgs(std::string const& user_id);

   // Used to asynchronously store messages on redis. On completion it
   // passes no event to the worker.
   template <class Iter>
   void store_chat_msg(std::string id, Iter begin, Iter end)
   {
      auto const key = cfg.msg_prefix + id;
      auto cmd_str = multi()
                   + incr(cfg.chat_msgs_counter_key)
                   + lpush(key, begin, end)
                   + expire(key, cfg.chat_msg_exp_time)
                   + exec();

      chat_pub_queue.push({request::ignore, {}});
      chat_pub_queue.push({request::ignore, {}});
      chat_pub_queue.push({request::ignore, {}});
      chat_pub_queue.push({request::ignore, {}});
      chat_pub_queue.push({request::ignore, {}});

      ss_chat_pub.send(std::move(cmd_str));
   }

   // Publishes the message on a redis channel where it is broadcasted
   // to all workers. Completes with
   //
   //    redis::request::publish
   //
   void pub_menu_msg(std::string const& msg, int id);

   // Requests a new post id from redis by increasing the last one.
   // Completes with
   //
   //    redis::request::post_id
   //
   void request_post_id();

   // Retrieves menu messages whose ids are greater than or equal
   // to begin. Completes with the event
   //
   //    redis::request::menu_msgs
   //
   void retrieve_menu_msgs(int begin);

   // Retrieves user messages. Completes with the event
   //
   //    redis::request::chat_messages
   //
   void retrieve_chat_msgs(std::string const& user_id);
   //
   // Requests a new user id from redis by increasing the last one.
   // Completes with
   //
   //    redis::request::user_id
   //
   void request_user_id();

   // Closes all stablished connections with redis.
   void disconnect();

   auto get_menu_pub_queue_size() const noexcept
      { return std::size(menu_pub_queue);}

   auto get_chat_pub_queue_size() const noexcept
      { return std::size(chat_pub_queue);}
};

}

