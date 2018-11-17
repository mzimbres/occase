#pragma once

#include <memory>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>

#include "config.hpp"
#include "server_session.hpp"

namespace rt
{

struct listener_cf {
   std::string ip;
   unsigned short port;
};

class mgr_arena;

class listener {
private:
   tcp::acceptor acceptor;
   std::vector<std::shared_ptr<mgr_arena>> const& arenas;
   std::vector<tcp::socket> sockets;
   long long next = 0;

   void do_accept();
   void on_accept(boost::system::error_code ec);

public:
   listener( listener_cf op
           , std::vector<std::shared_ptr<mgr_arena>> const& arenas_
           , boost::asio::io_context& ioc);
   ~listener();
   void run();
   void shutdown() {acceptor.cancel();}
};

}

