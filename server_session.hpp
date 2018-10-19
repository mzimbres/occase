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

class server_session :
   public std::enable_shared_from_this<server_session> {
private:
   enum class ping_pong
   { ping_sent
   , pong_received
   , unset
   };

   beast::websocket::stream<tcp::socket> ws;
   asio::strand<asio::io_context::executor_type> strand;
   asio::steady_timer timer;
   beast::multi_buffer buffer;

   server_mgr& mgr;
   std::queue<std::string> msg_queue;
   ping_pong pp_state = ping_pong::unset;
   bool closing = false;

   // This variable is called in this function only in the destructor.
   // So I do not any possibility of race condition arising with it.
   std::string user_id;

   // Not used by the session. So there is also not possible race.
   std::string sms;

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
   void do_send(std::string msg);
   void do_accept();

public:
   explicit
   server_session(tcp::socket socket, server_mgr& mgr);
   ~server_session();

   void accept();
   void send(std::string msg);
   void shutdown();

   void promote()
      { sms.clear(); }
   void set_sms(std::string sms_)
      {sms = std::move(sms_);}
   auto const& get_sms() const
      { return sms; }
   void set_id(std::string id)
      {user_id = std::move(id);};
   auto const& get_id() const noexcept
      {return user_id;}
   auto is_waiting_sms() const noexcept
      {return !std::empty(user_id) && !std::empty(sms);};
   auto is_auth() const noexcept
      {return !std::empty(user_id) && std::empty(sms);};
   auto is_waiting_auth() const noexcept
      {return std::empty(user_id) && std::empty(sms);};
};

