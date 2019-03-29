#pragma once

#include <deque>
#include <chrono>
#include <memory>
#include <atomic>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "config.hpp"

namespace rt
{

class worker;
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

struct proxy_session {
   // We have to use a weak pointer here. A share_ptr does not work.
   // Even when the proxy_session object is explicitly killed. The
   // object is itself killed only after the last weak_ptr is
   // destructed. The shared_ptr will live that long in that case.
   std::weak_ptr<server_session> session;
};

class server_session :
   public std::enable_shared_from_this<server_session> {
private:
   enum class ping_pong
   { ping_sent
   , pong_received
   , unset
   };

   beast::websocket::stream<net::ip::tcp::socket> ws;
   net::strand<net::io_context::executor_type> strand;
   net::steady_timer timer;
   beast::multi_buffer buffer;

   worker& worker_;

   struct msg_entry {
      std::string msg;
      std::shared_ptr<std::string> menu_msg;
      bool persist;
   };

   // TODO: Make this a priority queue.
   std::deque<msg_entry> msg_queue;

   ping_pong pp_state = ping_pong::unset;
   bool closing = false;

   std::shared_ptr<proxy_session> psession;

   std::string user_id;
   std::string code;

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
   void do_accept();

public:
   explicit
   server_session(net::ip::tcp::socket socket, worker& w);
   ~server_session();

   void accept();

   // Messages for which persist is true will be persisted on the
   // database and sent to the user next time he reconnects.
   void send(std::string msg, bool persist);
   void send_menu_msg(std::shared_ptr<std::string> msg);
   void shutdown();

   void promote()
      { code.clear(); }
   void set_code(std::string code_)
      { code = std::move(code_);}
   auto const& get_code() const
      { return code; }
   void set_id(std::string id)
      { user_id = std::move(id); };
   auto const& get_id() const noexcept
      { return user_id;}
   auto is_waiting_code() const noexcept
      { return !std::empty(user_id) && !std::empty(code);};
   auto is_auth() const noexcept
      { return !std::empty(user_id) && std::empty(code);};
   auto is_waiting_auth() const noexcept
      { return std::empty(user_id) && std::empty(code);};

   std::weak_ptr<proxy_session> get_proxy_session(bool new_session);
};

}

