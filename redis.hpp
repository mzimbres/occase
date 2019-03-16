#pragma once

#include <string>

#include "redis_session.hpp"

namespace rt::redis
{

enum class request
{ get_menu
, unsol_user_msgs
, ignore
, publish
, unsolicited_publish
, pub_counter
, get_user_msg
};

struct req_item {
   request req;
   std::string user_id;
};

// These are the names used inside redis to separate the keys and
// make it easier to use widecards.
struct db_cf {
   // The name of the channel where all *publish* commands will be
   // sent.
   std::string menu_channel;

   // This is the key in redis holding the menus in json format.
   std::string menu_key;

   // The prefix added to all keys that store user messages. The final
   // key will be a composition of this prefix and the user id
   // separate by a ":" e.g. msg:mzimbres@gmail.com.
   std::string msg_prefix;

   // This is the redis keyspace prefix. When a key is touched redis
   // sent us notification. This is how a worker gets notified that it
   // has to retrieve messages from the database for this user.
   std::string notify_prefix {"__keyspace@"};

   // The name of the key used to store the number of menu messages
   // sent so far.
   std::string menu_msgs_counter_key;

   // The name of the key used to store the number of user messages
   // sent so far.
   std::string user_msgs_counter_key;

   // The key used to store menu_msgs in redis.
   std::string menu_msgs_key;

   // Expiration time for user message keys. Keys will be deleted on
   // expiration.
   int user_msg_exp_time {3600};
};

struct config {
   session_cf ss_cf;
   db_cf cf;
};

// Facade class to manage the communication with redis and hide some
// uninteresting details from the workers.
class facade {
public:
   using msg_handler_type =
      std::function<void ( std::vector<std::string> const&
                         , req_item const&)>;

private:
   // Session used to subscribe to menu messages. No commands should
   // be posted here (with the exception of subscribe and
   // unsubscribe).
   session ss_menu_sub;

   // Session that deals with the publication of menu commands.  On
   // startup it will also be used to retrieve menu messages to load
   // the workers.
   session ss_menu_pub;
   std::queue<request> menu_pub_queue;

   // The session used to subscribe to keyspace notifications e.g.
   // when the user receives a message. Again, no commands should be
   // posted here (with the exception of subscribe and unsubscribe)
   session ss_user_sub;

   // Session used to store and retrieve user messages whose notification
   // arrived on session ss_user_sub.
   session ss_user_pub;
   std::queue<req_item> user_pub_queue;

   // TODO: We need a session to subscribe to changes in the menu.
   // Instead we may also consider using signals to trigger it.

   db_cf const cf;

   msg_handler_type worker_handler;

   // Callbacks called when a message is received from redis.
   void on_menu_sub( boost::system::error_code const& ec
                   , std::vector<std::string> const& data);

   void on_menu_pub( boost::system::error_code const& ec
                   , std::vector<std::string> const& data);

   void on_user_sub( boost::system::error_code const& ec
                   , std::vector<std::string> const& data);

   void on_user_pub( boost::system::error_code const& ec
                   , std::vector<std::string> const& data);

   // Callback called when sessions connect to redis server.
   void on_menu_sub_conn();
   void on_menu_pub_conn();
   void on_user_sub_conn();
   void on_user_pub_conn();

public:
   facade(config const& cf, net::io_context& ioc);

   // Incomming message will complete on this handler with
   // one of the possible codes defined in redis::request.
   void set_on_msg_handler(msg_handler_type h)
      { worker_handler = h; }

   void run();

   // Retrieves the menu asynchronously. The callback will complete
   // with redis::request::get_menu.
   void async_retrieve_menu();

   // Instructs redis to notify us upon any change to the given
   // user_id. Once a notification arrives the server proceeds with
   // the retrieval of the message, which may be more than one by the
   // time we get to it. User messages will complete on workers
   // callback with
   // 
   //    redis::request::unsol_user_msgs
   //
   void sub_to_user_msgs(std::string const& user_id);

   // Usubscribe to the notifications to the key. On completion it
   // passes no event to the worker.
   void unsub_to_user_msgs(std::string const& user_id);

   // Used to asynchronously store messages on redis. On completion it
   // passes no event to the worker.
   template <class Iter>
   void store_user_msg(std::string id, Iter begin, Iter end)
   {
      // Should we also impose a maximum length?

      auto const key = cf.msg_prefix + id;
      auto cmd_str = multi()
                   + incr(cf.user_msgs_counter_key)
                   + rpush(key, begin, end)
                   + expire(key, cf.user_msg_exp_time)
                   + exec();

      user_pub_queue.push({request::ignore, {}});
      user_pub_queue.push({request::ignore, {}});
      user_pub_queue.push({request::ignore, {}});
      user_pub_queue.push({request::ignore, {}});
      user_pub_queue.push({request::ignore, {}});

      ss_user_pub.send(std::move(cmd_str));
   }

   // Publishes the message on a redis channel where it is broadcasted
   // to all workers. TODO: Before posting it on the channel we have
   // to store it somewhere in redis, so that they can be read when a
   // new server is restarted. Completes with
   //
   //    redis::request::publish
   //
   void pub_menu_msg(std::string const& msg, int id);

   void request_pub_id();

   // Closes the connections to redis.
   void disconnect();
};

}

