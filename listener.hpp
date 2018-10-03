#pragma once

#include <memory>

#include <boost/asio.hpp>

#include "config.hpp"
#include "server_session.hpp"

// TODO: Understand why a forward declaration is no working.
//class server_mgr;
#include "server_mgr.hpp"

class listener : public std::enable_shared_from_this<listener> {
private:
   tcp::acceptor acceptor;
   tcp::socket socket;
   std::shared_ptr<server_mgr> mgr;
   std::shared_ptr<const server_session_timeouts> timeouts;

public:
   listener( boost::asio::io_context& ioc
           , tcp::endpoint endpoint
           , std::shared_ptr<server_mgr> mgr_
           , std::shared_ptr<const server_session_timeouts> timeouts);
   void run();
   void do_accept();
   void on_accept(boost::system::error_code ec);
   void stop() {acceptor.cancel();}
};

