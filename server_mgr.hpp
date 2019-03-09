#pragma once

#include <stack>
#include <queue>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>

#include "redis.hpp"
#include "config.hpp"
#include "channel.hpp"
#include "json_utils.hpp"
#include "menu.hpp"
#include "server_session.hpp"

namespace rt
{

struct server_mgr_cf {
   redis::config redis_cf;
   session_timeouts timeouts;
   int ch_cleanup_rate; 
   int ch_max_posts; 
};

struct sessions_stats {
   int number_of_sessions {0};
};

// We have to store publish items in this queue while we wait for
// the pub id that were requested from redis.
struct pub_queue_item {
   std::weak_ptr<server_session> session;
   pub_item item;
   std::string user_id;
};

class server_mgr {
private:
   net::io_context ioc {1};
   net::signal_set signals;

   // Maps a user id (telephone, email, etc.) to the user session.  We
   // keep only a weak reference to the session.
   std::unordered_map< std::string
                     , std::weak_ptr<server_session>
                     > sessions;

   // Maps a channel id to the corresponding channel object.
   std::unordered_map<std::uint64_t, channel> channels;

   session_timeouts const timeouts;
   sessions_stats stats;

   redis::facade db;

   std::vector<menu_elem> menus;

   net::steady_timer stats_timer;

   // Queue of user posts waiting for an id that has been requested
   // from redis.
   std::queue<pub_queue_item> pub_wait_queue;

   // This is the frequency we will be cleaning up the channel if no
   // publish activity is observed.
   int ch_cleanup_rate; 

   // Max number of messages stored in the each channel.
   int ch_max_posts; 

   void do_stats_logger();

   void init();

   void shutdown();

   ev_res on_user_register( json const& j
                          , std::shared_ptr<server_session> s);

   ev_res on_user_login( json const& j
                       , std::shared_ptr<server_session> s);

   ev_res on_user_code_confirm( json const& j
                              , std::shared_ptr<server_session> s);

   ev_res on_user_subscribe( json const& j
                           , std::shared_ptr<server_session> s);

   ev_res on_user_msg( std::string msg, json const& j
                     , std::shared_ptr<server_session> s);

   ev_res on_user_publish( json j
                         , std::shared_ptr<server_session> s);

   void on_db_user_msgs( std::string const& user_id
                       , std::vector<std::string> const& msgs) const;

   void on_db_get_menu(std::string const& data);
   void on_db_unsol_pub(std::string const& data);
   void on_db_msg_handler( std::vector<std::string> const& resp
                         , redis::req_item const& cmd);
   void on_db_pub_counter(std::string const& data);

public:
   server_mgr(server_mgr_cf cf);

   // When a server session dies, there are many things that must be
   // cleaned up or persisted. This function is responsible for that
   // TODO: Make it noexcept.
   void on_session_dtor( std::string const& id
                       , std::vector<std::string> msgs);

   ev_res on_message(std::shared_ptr<server_session> s, std::string msg);

   auto const& get_timeouts() const noexcept {return timeouts;}
   auto& get_stats() noexcept {return stats;}
   auto& get_io_context() noexcept {return ioc;}
   void run() noexcept;
};

}

