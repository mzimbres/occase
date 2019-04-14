#pragma once

#include <stack>
#include <queue>
#include <vector>
#include <memory>
#include <string>
#include <random>
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

class pwd_gen {
private:
   std::mt19937 gen;
   std::uniform_int_distribution<int> dist;

public:
   pwd_gen();

   std::string operator()(int size);
};

struct worker_only_cfg {
   // The maximum number of channels that is allowed to be sent to the
   // user on subscribe.
   int max_posts_on_sub; 
};

struct worker_cfg {
   redis::config db;
   session_cfg session;
   worker_only_cfg worker;
   channel_cfg channel;
};

struct sessions_stats {
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

class worker {
private:
   // The size of the passwords sent to the app.
   static const auto pwd_size = 10;

   // This worker id is needed to put individual worker log messages
   // apart.
   int const id;

   worker_only_cfg const cfg;
   channel_cfg const ch_cfg;

   net::io_context& ioc_;

   // Maps a user id (telephone, email, etc.) to the user session.  We
   // keep only a weak reference to the session.
   std::unordered_map< std::string
                     , std::weak_ptr<worker_session>
                     > sessions;

   // Maps a channel id to the corresponding channel object.
   std::unordered_map<std::uint64_t, channel> channels;

   session_cfg const timeouts;
   sessions_stats stats;

   redis::facade db;

   std::vector<menu_elem> menus;

   // Queue of user posts waiting for an id that has been requested
   // from redis.
   std::queue<post_queue_item> post_queue;

   // Queue with sessions waiting for a user ids that are retrieved
   // from redis.
   std::queue<reg_queue_item> reg_queue;

   int last_post_id = 0;
   pwd_gen pwdgen;

   void init();
   void create_channels(std::vector<menu_elem> const& menus);

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
   auto const& get_timeouts() const noexcept
      { return timeouts;}
   auto& get_ws_stats() noexcept
      { return stats;}
   auto const& get_ws_stats() const noexcept
      { return stats; }
   void shutdown();
   auto get_id() const noexcept
      { return id;}
   auto const& get_db() const noexcept
      { return db;}
};

}

