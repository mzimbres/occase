#pragma once

#include <queue>
#include <array>
#include <thread>
#include <vector>
#include <memory>
#include <string>
#include <numeric>
#include <unordered_map>

#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>

#include "net.hpp"
#include "menu.hpp"
#include "utils.hpp"
#include "redis.hpp"
#include "crypto.hpp"
#include "channel.hpp"
#include "json_utils.hpp"
#include "stats_server.hpp"
#include "acceptor_mgr.hpp"
#include "db_session.hpp"

namespace rt
{

struct core_cfg {
   // The maximum number of channels that is allowed to be sent to the
   // user on subscribe.
   int max_posts_on_sub; 

   // The size of the password sent to the app when it registers.
   int pwd_size; 

   // Websocket port.
   unsigned short port;

   // The size of the tcp backlog.
   int max_listen_connections;

   // The key used to generate authenticated filenames that will be
   // user in the image server.
   std::string img_key;
};

struct worker_cfg {
   redis::config db;
   ws_timeouts timeouts;
   core_cfg core;
   channel_cfg channel;
   stats_server_cfg stats;
};

struct ws_stats {
   int number_of_sessions {0};
};

// We have to store publish items in this queue while we wait for
// the pub id that were requested from redis.
struct post_queue_item {
   std::weak_ptr<db_session> session;
   post item;
};

struct pwd_queue_item {
   std::weak_ptr<db_session> session;
   std::string pwd;
};

struct worker_stats {
   int number_of_sessions = 0;
   int worker_post_queue_size = 0;
   int worker_reg_queue_size = 0;
   int worker_login_queue_size = 0;
   int db_post_queue_size = 0;
   int db_chat_queue_size = 0;
};

std::ostream& operator<<(std::ostream& os, worker_stats const& stats);

class worker {
private:
   net::io_context ioc {1};

   core_cfg const cfg;

   // There are hundreds of thousends of channels typically, so we
   // store the configuration only once here.
   channel_cfg const ch_cfg;

   ws_timeouts const ws_timeouts_;
   ws_stats ws_stats_;

   // Maps a user id in to a websocket session.
   std::unordered_map< std::string
                     , std::weak_ptr<db_session>
                     > sessions;

   // The channels are stored in a vector and the channel hash codes
   // separately in another vector. The last element in the
   // channel_hashes is used as sentinel to perform fast linear
   // searches.
   std::vector<std::uint64_t> channel_hashes;
   std::vector<channel> channels;

   // Apps that do not register for any product channels (the seconds
   // item in the menu) will receive posts from any channel. This is
   // the channel that will store the web socket channels for this
   // case.
   channel none_channel;

   // Facade to redis.
   redis::facade db;

   menu_elems_array_type menu;

   // Queue of user posts waiting for an id that has been requested
   // from redis.
   std::queue<post_queue_item> post_queue;

   // Queue with sessions waiting for a user id that are retrieved
   // from redis.
   std::queue<pwd_queue_item> reg_queue;

   // Queue of users waiting to be checked for login.
   std::queue<pwd_queue_item> login_queue;

   // The last post id that this channel has received from reidis
   // pubsub menu channel.
   int last_post_id = 0;

   // Generates passwords that are sent to the app.
   pwd_gen pwdgen;

   // Accepts websocket connections.
   acceptor_mgr<db_session> acceptor;

   // Provides some statistics about the server.
   stats_server sserver;

   // Signal handler.
   net::signal_set signal_set;

   void init();
   void create_channels(menu_elems_array_type const& menu);

   ev_res on_app_login( json const& j
                      , std::shared_ptr<db_session> s);
   ev_res on_app_register( json const& j
                         , std::shared_ptr<db_session> s);
   ev_res on_app_subscribe( json const& j
                          , std::shared_ptr<db_session> s);
   ev_res on_app_chat_msg( json j
                         , std::shared_ptr<db_session> s);
   ev_res on_app_presence( json j
                         , std::shared_ptr<db_session> s);
   ev_res on_app_publish( json j
                        , std::shared_ptr<db_session> s);
   ev_res on_app_del_post( json j
                         , std::shared_ptr<db_session> s);
   ev_res on_app_filenames( json j
                          , std::shared_ptr<db_session> s);

   // Handlers for events we receive from the database.
   void on_db_chat_msg( std::string const& user_id
                      , std::vector<std::string> msgs);
   void on_db_menu(std::string const& data);
   void on_db_channel_post(std::string const& data);
   void on_db_presence(std::string const& user_id, std::string data);
   void on_db_posts(std::vector<std::string> const& msgs);
   void on_db_event( std::vector<std::string> resp
                   , redis::req_item const& cmd);
   void on_db_post_id(std::string const& data);
   void on_db_publish();
   void on_db_menu_connect();
   void on_db_user_id(std::string const& msg);
   void on_db_register();
   void on_db_user_data(std::vector<std::string> const& data) noexcept;

   void on_signal(boost::system::error_code const& ec, int n);

public:
   worker(worker_cfg cfg);

   void on_session_dtor( std::string const& id
                       , std::vector<std::string> msgs);

   ev_res on_app( std::shared_ptr<db_session> s
                , std::string msg) noexcept;

   auto const& get_timeouts() const noexcept
      { return ws_timeouts_;}
   auto& get_ws_stats() noexcept
      { return ws_stats_;}
   auto const& get_ws_stats() const noexcept
      { return ws_stats_; }
   void shutdown();
   worker_stats get_stats() const noexcept;
   auto& get_ioc() const noexcept
      { return ioc; }

   void run();
};

}

