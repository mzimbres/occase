#pragma once

#include <memory>

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>

#include "config.hpp"
#include "server_session.hpp"

class server_mgr;

class listener : public std::enable_shared_from_this<listener> {
private:
   tcp::acceptor acceptor;
   tcp::socket socket;
   std::shared_ptr<server_mgr> mgr;
   std::shared_ptr<const server_session_timeouts> timeouts;
   std::shared_ptr<sessions_stats> stats;
   boost::asio::steady_timer session_stats_timer;

   void do_stats_logger();

public:
   listener( boost::asio::io_context& ioc
           , tcp::endpoint endpoint
           , std::shared_ptr<server_mgr> mgr_
           , std::shared_ptr<const server_session_timeouts> timeouts);
   ~listener();
   void run();
   void do_accept();
   void on_accept(boost::system::error_code ec);
   void stop();
};

