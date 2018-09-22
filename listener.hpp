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
   std::shared_ptr<server_mgr> sd;
   server_session_config cf;

public:
   listener( boost::asio::io_context& ioc
           , tcp::endpoint endpoint
           , std::shared_ptr<server_mgr> sd_
           , server_session_config cf_);
   void run();
   void do_accept();
   void on_accept(boost::system::error_code ec);
   void stop() {acceptor.cancel();}
};

