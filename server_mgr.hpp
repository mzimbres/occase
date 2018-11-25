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

namespace rt
{

class server_session;

enum class ev_res
{ register_ok
, register_fail
, login_ok
, login_fail
, code_confirmation_ok
, code_confirmation_fail
, subscribe_ok
, subscribe_fail
, unsubscribe_ok
, unsubscribe_fail
, publish_ok
, publish_fail
, user_msg_ok
, user_msg_fail
, unknown
};

struct session_timeouts {
   std::chrono::seconds auth {2};
   std::chrono::seconds code {2};
   std::chrono::seconds handshake {2};
   std::chrono::seconds pong {2};
   std::chrono::seconds close {2};
};

struct server_mgr_cf {
   redis::config redis_cf;
   session_timeouts timeouts;
};

struct sessions_stats {
   int number_of_sessions {0};
};

class server_mgr {
private:
   net::io_context ioc {1};
   net::signal_set signals;

   // Maps a user id (telephone, email, etc.) to the user session.  We
   // keep only a weak reference to the session to avoid.
   std::unordered_map< std::string
                     , std::weak_ptr<server_session>
                     > sessions;

   // Maps a channel id to the corresponding channel object.
   std::unordered_map<std::string, channel> channels;

   session_timeouts const timeouts;
   sessions_stats stats;

   redis::facade db;
   std::string menu;

   net::steady_timer stats_timer;

   void redis_menu_msg_handler( boost::system::error_code const& ec
                              , std::vector<std::string> const& resp
                              , redis::req_data const& cmd);
   void redis_key_not_handler( boost::system::error_code const& ec
                             , std::vector<std::string> const& resp
                             , redis::req_data const& cmd);
   void redis_pub_msg_handler( boost::system::error_code const& ec
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
   ev_res on_unsubscribe(json const& j, std::shared_ptr<server_session> s);
   ev_res on_user_msg( std::string msg, json const& j
                     , std::shared_ptr<server_session> s);
   ev_res on_publish( std::string msg, json const& j
                    , std::shared_ptr<server_session> s);


public:
   server_mgr(server_mgr_cf cf);
   void release_auth_session(std::string const& id);
   ev_res on_message(std::shared_ptr<server_session> s, std::string msg);

   auto const& get_timeouts() const noexcept {return timeouts;}
   auto& get_stats() noexcept {return stats;}
   auto& get_io_context() noexcept {return ioc;}
   void run() noexcept;
};

}

