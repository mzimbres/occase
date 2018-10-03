#pragma once

#include <queue>
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
#include "server_mgr.hpp"

struct server_session_timeouts {
   std::chrono::seconds auth {2};
   std::chrono::seconds sms {2};
   std::chrono::seconds handshake {2};
   std::chrono::seconds pong {2};
};

struct sessions_stats {
   std::atomic<int> number_of_sessions {0};
};

struct session_shared {
   std::shared_ptr<server_mgr> mgr;
   std::shared_ptr<const server_session_timeouts> timeouts;
   std::shared_ptr<sessions_stats> stats;
};

enum class ping_pong
{ ping_sent
, pong_received
, unset
};

// TODO: Introduce close frames timeout.
class server_session :
   public std::enable_shared_from_this<server_session> {
private:
   websocket::stream<tcp::socket> ws;
   boost::asio::strand<boost::asio::io_context::executor_type> strand;
   boost::asio::steady_timer timer;
   boost::beast::multi_buffer buffer;

   session_shared shared;
   std::string user_id;
   std::string sms;
   std::queue<std::string> msg_queue;
   ping_pong pp_state = ping_pong::unset;
   bool closing = false;

   void do_read();
   void do_write(std::string msg);
   void on_read( boost::system::error_code ec
               , std::size_t bytes_transferred);
   void on_close(boost::system::error_code ec);
   void on_accept(boost::system::error_code ec);
   void on_write( boost::system::error_code ec
                , std::size_t bytes_transferred);
   void handle_ev(ev_res r);
   void do_ping();
   void do_pong_wait();

public:
   explicit
   server_session(tcp::socket socket, session_shared shared_);
   ~server_session();

   void do_accept();
   void set_sms(std::string sms_) {sms = std::move(sms_);}
   auto const& get_sms() const { return sms; }
   void set_user_id(std::string id) {user_id = id;};
   auto get_user_id() const noexcept {return user_id;}
   void send_msg(std::string msg);
   void promote() { sms.clear(); }
   auto is_waiting_sms() const noexcept
   {return !std::empty(user_id) && !std::empty(sms);};
   auto is_auth() const noexcept
   {return !std::empty(user_id) && std::empty(sms);};
   auto is_waiting_auth() const noexcept
   {return std::empty(user_id) && std::empty(sms);};
   void do_close();
};

