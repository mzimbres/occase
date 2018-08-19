#pragma once

#include <memory>

#include <boost/asio.hpp>

#include "config.hpp"

// TODO: Understand why a forward declaration is no working.
//class server_data;
#include "server_data.hpp"

class listener : public std::enable_shared_from_this<listener> {
private:
   tcp::acceptor acceptor;
   tcp::socket socket;
   std::shared_ptr<server_data> sd;

public:
   listener( boost::asio::io_context& ioc
           , tcp::endpoint endpoint
           , std::shared_ptr<server_data> sd_);
   void run();
   void do_accept();
   void on_accept(boost::system::error_code ec);
};

