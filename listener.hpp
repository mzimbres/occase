#pragma once

#include <memory>

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>

#include "config.hpp"
#include "server_session.hpp"

struct server_op {
   bool help = false;
   std::string ip;
   unsigned short port;
   int auth_timeout;
   int sms_timeout;
   int handshake_timeout;
   int pong_timeout;
   int close_frame_timeout;

   auto get_timeouts() const noexcept
   {
      return session_timeouts
      { std::chrono::seconds {auth_timeout}
      , std::chrono::seconds {sms_timeout}
      , std::chrono::seconds {handshake_timeout}
      , std::chrono::seconds {pong_timeout}
      , std::chrono::seconds {close_frame_timeout}
      };
   }
};

class server_mgr;

class listener : public std::enable_shared_from_this<listener> {
private:
   tcp::acceptor acceptor;
   tcp::socket socket;
   std::shared_ptr<server_mgr> mgr;
   boost::asio::steady_timer session_stats_timer;

   void do_stats_logger();

public:
   listener( server_op op, boost::asio::io_context& ioc
           , std::shared_ptr<server_mgr> mgr_);
   ~listener();
   void run();
   void do_accept();
   void on_accept(boost::system::error_code ec);
   void stop();
};

