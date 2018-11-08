#pragma once

#include <memory>

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>

#include "config.hpp"
#include "server_session.hpp"

namespace rt
{

struct server_op {
   std::string ip;
   unsigned short port;
   server_mgr_cf mgr;
};

class server_mgr;

class listener {
private:
   boost::asio::signal_set signals;
   tcp::acceptor acceptor;
   tcp::socket socket;
   server_mgr mgr;

   void do_accept();
   void on_accept(boost::system::error_code ec);

public:
   listener(server_op op, boost::asio::io_context& ioc);
   ~listener();
   void run();
};

}

