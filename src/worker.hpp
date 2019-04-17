#pragma once

#include <stack>
#include <queue>
#include <vector>
#include <memory>
#include <string>
#include <numeric>
#include <unordered_map>

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>

#include "menu.hpp"
#include "redis.hpp"
#include "config.hpp"
#include "channel.hpp"
#include "json_utils.hpp"
#include "worker_session.hpp"

namespace rt
{

struct worker_only_cfg {
   // The maximum number of channels that is allowed to be sent to the
   // user on subscribe.
   int max_posts_on_sub; 
};

struct worker_cfg {
   redis::config db;
   ws_ss_timeouts timeouts;
   worker_only_cfg worker;
   channel_cfg channel;
};

struct ws_ss_stats {
   int number_of_sessions {0};
};

// We have to store publish items in this queue while we wait for
// the pub id that were requested from redis.
struct post_queue_item {
   std::weak_ptr<worker_session> session;
   post item;
   std::string user_id;
};

struct reg_queue_item {
   std::weak_ptr<worker_session> session;
   std::string pwd;
};

struct login_queue_item {
   std::weak_ptr<worker_session> session;
   std::string pwd;
   bool send_menu;
};

class worker {
private:
   net::io_context& ioc_;

   // The size of the passwords sent to the app.
   static const auto pwd_size = 10;

   // This worker id.
   int const id;

   worker_only_cfg const cfg;

   // There are hundreds of thousends of channels typically, so we
   // store the configuration only once here.
   channel_cfg const ch_cfg;

   ws_ss_timeouts const ws_ss_timeouts_;
   ws_ss_stats ws_ss_stats_;

   // Maps a user id in to a websocket session.
   std::unordered_map< std::string
                     , std::weak_ptr<worker_session>
                     > sessions;

   // Maps a channel hash code into its corresponding channel object.
   std::unordered_map<std::uint64_t, channel> channels;

   redis::facade db;

   std::vector<menu_elem> menu;

   // Queue of user posts waiting for an id that has been requested
   // from redis.
   std::queue<post_queue_item> post_queue;

   // Queue with sessions waiting for a user ids that are retrieved
   // from redis.
   std::queue<reg_queue_item> reg_queue;

   // Queue of users waiting to be checked for login.
   std::queue<login_queue_item> login_queue;

   int last_post_id = 0;
   pwd_gen pwdgen;

   std::hash<std::string> const hash_func {};

   void init();
   void create_channels(std::vector<menu_elem> const& menu);

   ev_res on_app_login( json const& j
                      , std::shared_ptr<worker_session> s);
   ev_res on_app_register( json const& j
                         , std::shared_ptr<worker_session> s);
   ev_res on_app_subscribe( json const& j
                          , std::shared_ptr<worker_session> s);
   ev_res on_app_chat_msg( std::string msg, json const& j
                         , std::shared_ptr<worker_session> s);
   ev_res on_app_publish( json j
                        , std::shared_ptr<worker_session> s);

   // Handlers for events we receive from the database.
   void on_db_chat_msg( std::string const& user_id
                      , std::vector<std::string> msgs);
   void on_db_menu(std::string const& data);
   void on_db_post(std::string const& data);
   void on_db_posts(std::vector<std::string> const& msgs);
   void on_db_event( std::vector<std::string> resp
                   , redis::req_item const& cmd);
   void on_db_post_id(std::string const& data);
   void on_db_publish();
   void on_db_menu_connect();
   void on_db_user_id(std::string const& msg);
   void on_db_register();
   void on_db_user_data(std::vector<std::string> const& data);

public:
   worker(worker_cfg cfg, int id_, net::io_context& ioc);

   // When a server session dies, there are many things that must be
   // cleaned up or persisted. This function is responsible for that
   // TODO: Make it noexcept.
   void on_session_dtor( std::string const& id
                       , std::vector<std::string> msgs);

   ev_res on_message(std::shared_ptr<worker_session> s, std::string msg);

   auto get_post_queue_size() const noexcept
      { return std::size(post_queue);}
   auto get_reg_queue_size() const noexcept
      { return std::size(reg_queue);}
   auto get_login_queue_size() const noexcept
      { return std::size(login_queue);}
   auto const& get_timeouts() const noexcept
      { return ws_ss_timeouts_;}
   auto& get_ws_stats() noexcept
      { return ws_ss_stats_;}
   auto const& get_ws_stats() const noexcept
      { return ws_ss_stats_; }
   void shutdown();
   auto get_id() const noexcept
      { return id;}
   auto const& get_db() const noexcept
      { return db;}
};

}

