#pragma once

#include <string>

#include "redis_session.hpp"

namespace rt::redis
{

enum class request
{ get_menu
, unsol_user_msgs
, ignore
, store_msg
, publish
, unsolicited_publish
, pub_counter
};

struct req_item {
   request req;
   std::string user_id;
};

// These are the namespaces used inside redis to separate the keys and
// make it easier to use widecards.
struct namespaces {
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
};

struct config {
   session_cf sessions;
   namespaces nms;
};

// Facade class to manage the communication with redis and hide some
// uninteresting details from the workers.
class facade {
public:
   using msg_handler_type =
      std::function<void ( std::vector<std::string> const&
                         , req_item const&)>;

private:
   // The session used to subscribe to menu messages.
   session menu_sub_session;

   // The session used for keyspace notifications e.g. when the user
   // receives a message.
   session msg_not;

   // Redis session to send general commands.
   session pub_session;
   std::queue<req_item> pub_ev_queue;

   namespaces nms;

   msg_handler_type worker_handler;

   // Retrieves user messages asynchronously. Called automatically and
   // not passed to worker.
   void msg_not_handler( boost::system::error_code const& ec
                       , std::vector<std::string> const& data
                       , std::string const& req);

   void pub_handler( boost::system::error_code const& ec
                   , std::vector<std::string> const& data
                   , std::string const& req);

   void sub_handler( boost::system::error_code const& ec
                   , std::vector<std::string> const& data
                   , std::string const& req);
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
   void subscribe_to_chat_msgs(std::string const& user_id);

   // Usubscribe to the notifications to the key. On completion it
   // calls no function on the worker side.
   void unsubscribe_to_chat_msgs(std::string const& user_id);

   // Used to asynchronously store messages on redis. Completes with
   //
   //   redis::request::store_msg
   //
   template <class Iter>
   void async_store_chat_msg( std::string id
                            , Iter begin
                            , Iter end)
   {
      auto const d = std::distance(begin, end);

      auto payload = make_cmd_header(2 + d)
                   + make_bulky_item("RPUSH")
                   + make_bulky_item(nms.msg_prefix + std::move(id));

      auto cmd_str = std::accumulate( begin
                                    , end
                                    , std::move(payload)
                                    , accumulator{});

      pub_session.send(std::move(cmd_str));
      pub_ev_queue.push({request::store_msg, {}});
   }

   // Publishes the message on a redis channel where it is broadcasted
   // to all workers. TODO: Before posting it on the channel we have
   // to store it somewhere in redis, so that they can be read when a
   // new server is restarted. Completes with
   //
   //    redis::request::publish
   //
   void publish_menu_msg(std::string msg);

   void request_pub_id();

   // Closes the connections to redis.
   void disconnect();
};

}

