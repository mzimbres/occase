#pragma once

#include <queue>
#include <thread>
#include <vector>
#include <memory>
#include <string>
#include <numeric>
#include <unordered_map>

#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>

#include "menu.hpp"
#include "utils.hpp"
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

   // The size of the password sent to the app when it registers.
   int pwd_size; 
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

struct worker_stats {
   int number_of_sessions = 0;
   int worker_post_queue_size = 0;
   int worker_reg_queue_size = 0;
   int worker_login_queue_size = 0;
   int db_post_queue_size = 0;
   int db_chat_queue_size = 0;
};

void add(worker_stats& a, worker_stats const& b) noexcept;

std::ostream& operator<<(std::ostream& os, worker_stats const& stats);

class worker {
private:
   net::io_context& ioc_;

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

   // Queue with sessions waiting for a user id that are retrieved
   // from redis.
   std::queue<reg_queue_item> reg_queue;

   // Queue of users waiting to be checked for login.
   std::queue<login_queue_item> login_queue;

   // The last post id that this channel has received from reidis
   // pubsub menu channel.
   int last_post_id = 0;

   // Generateds passwords that are sent to the app.
   pwd_gen pwdgen;

   // Before we store the password in the database we hash it.
   // TODO: Implement a platform independent has function or use from
   // a package. Use salt.
   std::hash<std::string> const hash_func {};

   void init();
   void create_channels(std::vector<menu_elem> const& menu);

   ev_res on_app_login( json const& j
                      , std::shared_ptr<worker_session> s);
   ev_res on_app_register( json const& j
                         , std::shared_ptr<worker_session> s);
   ev_res on_app_subscribe( json const& j
                          , std::shared_ptr<worker_session> s);
   ev_res on_app_chat_msg( json j
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
   void on_db_user_data(std::vector<std::string> const& data) noexcept;

public:
   worker(worker_cfg cfg, int id_, net::io_context& ioc);

   void on_session_dtor( std::string const& id
                       , std::vector<std::string> msgs);

   ev_res on_app( std::shared_ptr<worker_session> s
                , std::string msg) noexcept;

   auto const& get_timeouts() const noexcept
      { return ws_ss_timeouts_;}
   auto& get_ws_stats() noexcept
      { return ws_ss_stats_;}
   auto const& get_ws_stats() const noexcept
      { return ws_ss_stats_; }
   void shutdown();
   auto get_id() const noexcept
      { return id;}
   worker_stats get_stats() const noexcept;
   auto& get_ioc() const noexcept
   { return ioc_; }
};

struct worker_arena {
   int id_;
   net::io_context ioc_ {1};
   net::signal_set signals_;
   worker worker_;
   std::thread thread_;

   worker_arena(worker_cfg const& cfg, int i);
   ~worker_arena();

   void on_signal(boost::system::error_code const& ec, int n);
   void run() noexcept;
};

}

