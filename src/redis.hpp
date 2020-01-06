#pragma once

#include <string>

#include <aedis/aedis.hpp>

#include "net.hpp"
#include "utils.hpp"

namespace occase
{

// Facade class to manage the communication with redis and hide
// uninteresting details from the workers.
class redis {
public:
   enum class events
   { channels
   , user_messages
   , post
   , posts
   , post_id
   , user_id
   , user_data
   , register_user
   , post_connect
   , channel_post
   , remove_post
   , user_msgs_priv
   , presence
   , update_post_deadline
   , ignore
   };

   struct response {
      events req;
      std::string user_id;
   };

   using msg_handler_type =
      std::function<void(std::vector<std::string>, response const&)>;
 
   // We will use two redis instances, one will hold posts, user logins,
   // post id counter, chat message counters, the other will deal only
   // with the storage of user messages when the user is offline. I think
   // this way we can avoid redis cluster since the amount of data in the
   // two cases are expected to be small. We can also use different
   // backup strategies, for the first case backup is very important, for
   // the second, not so much.
   struct config {
      // Redis session config for everything related to posts.
      aedis::session::config ss_post;

      // Redis session config for user credentials and data.
      aedis::session::config ss_user;

      // Redis session config for user messages.
      aedis::session::config ss_msgs;

      // The name of the channel where *publish* commands are be sent.
      std::string menu_channel_key;

      // Key in redis holding the menu in json format.
      std::string channels_key;

      // The key used to store posts in redis.
      std::string posts_key;

      // The prefix of chat message keys. The complete key will be a
      // composition of this prefix and the user id separate by a ":"
      // e.g. msg:102 where 102 is the user id, acquired on registration.
      std::string chat_msg_prefix;

      // This prefix will be used to form the channel where presence
      // messages sent to the user will be published e.g. prefix:102.
      // We use channel for presence since it does not have to be
      // persisted.
      std::string presence_channel_prefix = "pc:";

      // Redis keyspace notification prefix. When a key is touched redis
      // sendd us a notification. This is how a worker gets notified that
      // it has to retrieve messages from the database for this user.
      std::string const notify_prefix {"__keyspace@"};

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

      // The prefix to every id holding user data (password for example).
      std::string user_data_prefix_key;

      // The channel where user FCM tokens should be published.
      std::string token_channel {"tokens"};

      // Expiration time for user message keys. Keys will be deleted on
      // expiration and all chat messages that have not been retrieved
      // are gone.
      int chat_msg_exp_time {3600};

      // The maximum number of chat messages a user is allowed to
      // accumulate on the server (when he is offline).
      int max_offline_chat_msgs {100};

      // To spare space in the redis server we can user shorter fields string.
      struct user_fields {
         std::string password {"a"};
         std::string allowed {"b"};
         std::string remaining {"c"};
         std::string deadline {"d"};
      };

      user_fields ufields;

      auto is_valid() const noexcept
      {
         auto ret = std::empty(ufields.password)
                  || std::empty(ufields.allowed)
                  || std::empty(ufields.remaining)
                  || std::empty(ufields.deadline);

         return !ret;
      }
   };

private:
   config const cfg_;

   // Redis sessions to deal with posts.
   aedis::session ss_post_sub_;
   aedis::session ss_post_pub_;
   std::queue<events> post_pub_queue_;

   // Redis sessions used to deal with user messages and presence.
   aedis::session ss_msgs_sub_;
   aedis::session ss_msgs_pub_;
   std::queue<response> msgs_pub_queue_;

   // Redis sessions used to deal with user credentials.
   aedis::session ss_user_pub_;
   std::queue<events> user_pub_queue_;

   msg_handler_type ev_handler_;

   void on_post_sub( boost::system::error_code const& ec
                   , std::vector<std::string> data);

   void on_post_pub( boost::system::error_code const& ec
                   , std::vector<std::string> data);

   void on_msgs_sub( boost::system::error_code const& ec
                   , std::vector<std::string> data);

   void on_msgs_pub( boost::system::error_code const& ec
                   , std::vector<std::string> data);

   void on_user_pub( boost::system::error_code const& ec
                   , std::vector<std::string> data);

   void on_post_sub_conn();
   void on_post_pub_conn();
   void on_msgs_sub_conn();
   void on_msgs_pub_conn();
   void on_user_pub_conn();

public:
   redis(config const& cfg, net::io_context& ioc);

   // Incomming message will complete on this handler with
   // one of the possible codes defined in redis::events.
   void set_event_handler(msg_handler_type h)
      { ev_handler_ = h; }

   void run();

   // Retrieves the menu asynchronously. Complete with
   //
   //    redis::events::channels.
   //
   void retrieve_channels();

   // Instructs redis to notify the worker on new messages to the
   // user.  Once a notification arrives the server proceeds with the
   // retrieval of the message, which may be more than one by the time
   // we get to it. User messages will complete on workers callback
   // with
   // 
   //    redis::events::user_messages
   //
   // Additionaly, this function also subscribes the worker to
   // presence messages, which completes with
   // 
   //    redis::events::presence
   //
   void on_user_online(std::string const& user_id);

   // Usubscribe to the notifications to the key. On completion it
   // passes no event to the worker.
   void on_user_offline(std::string const& user_id);

   // Used to asynchronously store messages on redis. On completion it
   // passes no event to the worker.
   template <class Iter>
   void
   store_chat_msg( std::string const& id
                 , Iter begin
                 , Iter end)
   {
      if (begin == end)
         return;

      using namespace aedis;
      auto const key = cfg_.chat_msg_prefix + id;
      auto cmd_str = multi()
                   + incr(cfg_.chat_msgs_counter_key)
                   + rpush(key, begin, end)
                   + expire(key, cfg_.chat_msg_exp_time)
                   + publish(cfg_.token_channel, *std::prev(end))
                   + exec();

      msgs_pub_queue_.push({events::ignore, {}});
      msgs_pub_queue_.push({events::ignore, {}});
      msgs_pub_queue_.push({events::ignore, {}});
      msgs_pub_queue_.push({events::ignore, {}});
      msgs_pub_queue_.push({events::ignore, {}});
      msgs_pub_queue_.push({events::ignore, {}});

      ss_msgs_pub_.send(std::move(cmd_str));
   }

   // Sends presence to the user with id id. Completes with
   //
   //    redis::events::presence
   //
   void
   send_presence( std::string const& id
                , std::string const& msg);

   // Adds the post in the sorted set containing all posts and
   // publishes the post on the channel where it is broadcasted
   // to all workers. Completes with
   //
   //    redis::events::post
   //
   void post(std::string const& msg, int id);

   // Removes the post under id from the sorted set and broadcasts the
   // cmd to the other workers. Completes with
   //
   //    redis::events::remove_post
   //
   void remove_post(int id, std::string const& cmd);

   // Requests a new post id from redis by increasing the last one.
   // Completes with
   //
   //    redis::events::post_id
   //
   void request_post_id();

   // Retrieves menu messages whose ids are greater than or equal
   // to begin. Completes with the event
   //
   //    redis::events::posts
   //
   void retrieve_posts(int begin);

   // Retrieves user messages. Completes with the event
   //
   //    redis::events::user_messages
   //
   void retrieve_messages(std::string const& user_id);
   //
   // Requests a new user id from redis by increasing the last one.
   // Completes with
   //
   //    redis::events::user_id
   //
   void request_user_id();

   // Register a user in the database. Completes with
   //
   //    redis::events::register_user
   //
   // n_allowed_posts is the number of posts the user is allowed to publish
   // until the deadline.
   void
   register_user( std::string const& user
                , std::string const& pwd
                , int n_allowed_posts
                , std::chrono::seconds deadline);

   // Retrieves the user password (and possibly other data in the
   // future) from the database. Completes with 
   //
   //   redis::events::user_data
   //
   void retrieve_user_data(std::string const& user_id);

   // Updates the user last post timestamp. Completes with
   //
   //   redis::events::update_post_deadline
   //
   void
   update_post_deadline( std::string const& user_id
                       , int n_allowed_posts
                       , std::chrono::seconds deadline);

   // Updates the number of remaining posts. Completes with
   //
   //   redis::events::ignore
   //
   void
   update_remaining( std::string const& user_id
                   , int remaining);

   // Publishes a user FCM token in the channel where occase-notify is
   // listening. Completes with
   //
   //   redis::events::ignore
   //
   void
   publish_token( std::string const& id
                , std::string const& token);

   // Cleanly quits all stablished connections with redis, uses the redis
   // QUIT command. This will cause redis to send all pending messages
   // before quitting the session.
   void disconnect();

   auto get_post_queue_size() const noexcept
      { return ssize(post_pub_queue_);}

   auto get_chat_queue_size() const noexcept
      { return ssize(msgs_pub_queue_);}
};

}

