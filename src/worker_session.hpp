#pragma once

#include <deque>
#include <chrono>
#include <memory>
#include <atomic>
#include <cstdint>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/container/static_vector.hpp>

#include "config.hpp"
#include "json_utils.hpp"

namespace rt
{

class worker;
class worker_session;

enum class ev_res
{ register_ok
, register_fail
, login_ok
, login_fail
, subscribe_ok
, subscribe_fail
, publish_ok
, publish_fail
, chat_msg_ok
, chat_msg_fail
, delete_ok
, delete_fail
, filenames_ok
, filenames_fail
, unknown
};

struct ws_timeouts {
   std::chrono::seconds login {2};
   std::chrono::seconds handshake {2};
   std::chrono::seconds pong {2};
   std::chrono::seconds close {2};
};

struct proxy_session {
   // We have to use a weak pointer here. A share_ptr doen't work.
   // Even when the proxy_session object is explicitly killed. The
   // object is itself killed only after the last weak_ptr is
   // destructed. The shared_ptr will live that long in that case.
   std::weak_ptr<worker_session> session;
};

class worker_session :
   public std::enable_shared_from_this<worker_session> {
private:
   enum class ping_pong
   { ping_sent
   , pong_received
   , unset
   };

   beast::websocket::stream<net::ip::tcp::socket> ws;
   net::steady_timer timer;
   beast::multi_buffer buffer;

   worker& worker_;

   struct msg_entry {
      std::string msg;
      std::shared_ptr<std::string> menu_msg;
      bool persist;
   };

   std::deque<msg_entry> msg_queue;

   ping_pong pp_state = ping_pong::unset;
   bool closing = false;

   std::shared_ptr<proxy_session> psession;

   std::string user_id;

   std::uint64_t any_of_features = 0;

   static constexpr auto menu_codes_size = 32;

   using menu_codes_type =
      boost::container::static_vector< std::uint64_t
                                     , menu_codes_size>;

   menu_codes_type menu_codes;

   void do_read();
   void do_write(std::string const& msg);
   void on_read( boost::system::error_code ec
               , std::size_t bytes_transferred);
   void on_close(boost::system::error_code ec);
   void on_accept(boost::system::error_code ec);
   void on_write( boost::system::error_code ec
                , std::size_t bytes_transferred);
   void handle_ev(ev_res r);
   void do_ping();
   void do_pong_wait();
   void do_close();
   void do_send(msg_entry entry);

public:

   explicit
   worker_session(net::ip::tcp::socket socket, worker& w);
   ~worker_session();

   void accept();

   // Messages for which persist is true will be persisted on the
   // database and sent to the user next time he reconnects.
   void send(std::string msg, bool persist);
   void send_post( std::shared_ptr<std::string> msg
                 , std::uint64_t hash_code
                 , std::uint64_t features);
   void shutdown();

   void set_id(std::string id)
      { user_id = std::move(id); };

   // Sets the any_of filter, used to filter posts sent with send_post
   // above. If the filter is non-null the post features will be
   // required to contain at least one bit set that is also set in the
   // argument passed here.
   void set_any_of_features(std::uint64_t o)
      { any_of_features = o; }
   void set_filter(std::vector<std::uint64_t> const& codes);
   auto const& get_id() const noexcept
      { return user_id;}
   auto is_logged_in() const noexcept
      { return !std::empty(user_id);};

   std::weak_ptr<proxy_session> get_proxy_session(bool make_new_session);
};

}

