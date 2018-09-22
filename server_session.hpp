#pragma once

#include <queue>
#include <chrono>
#include <memory>

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "config.hpp"
#include "server_mgr.hpp"

// TODO: Think whether having one such struct per session consumes
// more memory than we want and whether we should store a pointer to a
// shared object instead.
struct server_session_config {
   std::chrono::seconds on_acc_timeout {2};
   std::chrono::seconds sms_timeout {2};
};

// TODO: Implementing control frames as in Beast advanced example.
class server_session :
   public std::enable_shared_from_this<server_session> {
private:
   websocket::stream<tcp::socket> ws;
   boost::asio::strand<boost::asio::io_context::executor_type> strand;
   boost::asio::steady_timer timer;
   boost::beast::multi_buffer buffer;

   server_session_config cf;
   std::shared_ptr<server_mgr> sd;
   index_type user_idx = -1;
   index_type login_idx = -1;
   std::string sms;
   std::queue<std::string> msg_queue;

   void do_read();
   void do_write(std::string msg);
   void do_close();
   void on_read( boost::system::error_code ec
               , std::size_t bytes_transferred);
   void on_close(boost::system::error_code ec);
   void on_accept(boost::system::error_code ec);
   void on_write( boost::system::error_code ec
                , std::size_t bytes_transferred);
   void on_timer(boost::system::error_code ec);
   void handle_ev(ev_res r);

public:
   explicit
   server_session( tcp::socket socket
                 , std::shared_ptr<server_mgr> sd_
                 , server_session_config cf_);
   ~server_session();

   void do_accept();
   void set_sms(std::string sms_) {sms = std::move(sms_);}
   auto const& get_sms() const { return sms; }
   void set_user(index_type idx) {user_idx = idx;};
   void set_login_idx(index_type idx) {login_idx = idx;};
   auto get_user_idx() const noexcept {return user_idx;}
   void send_msg(std::string msg);
   void promote() { std::swap(user_idx, login_idx ); }
   auto is_waiting_sms() const noexcept
   {return login_idx != -1 && user_idx == -1;};
   auto is_auth() const noexcept
   {return login_idx == -1 && user_idx != -1;};
   auto is_waiting_auth() const noexcept
   {return login_idx == -1 && user_idx == -1;};
};

