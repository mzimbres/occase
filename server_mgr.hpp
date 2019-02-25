#pragma once

#include <stack>
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
   int channel_cleanup_frequency; 
};

struct sessions_stats {
   int number_of_sessions {0};
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

   // This is the frequency we will be cleaning up the channel if no
   // publish activity is observed.
   int ch_cleanup_freq; 

   void redis_on_msg_handler( boost::system::error_code const& ec
                            , std::vector<std::string> const& resp
                            , redis::req_data const& cmd);
   void do_stats_logger();

   void init();

   void shutdown();

   ev_res on_register(json const& j, std::shared_ptr<server_session> s);
   ev_res on_login(json const& j, std::shared_ptr<server_session> s);
   ev_res on_code_confirmation( json const& j
                              , std::shared_ptr<server_session> s);
   ev_res on_subscribe(json const& j, std::shared_ptr<server_session> s);
   ev_res on_user_msg( std::string msg, json const& j
                     , std::shared_ptr<server_session> s);
   ev_res on_publish(json j, std::shared_ptr<server_session> s);

   void on_redis_retrieve_msgs( std::vector<std::string> const& data
                              , redis::req_data const& req);

   void on_redis_get_menu(std::string const& data);

   void on_redis_unsol_pub(std::string const& data);

   void on_redis_unsol_key_not( std::vector<std::string> const& data
                              , redis::req_data const& req);

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

