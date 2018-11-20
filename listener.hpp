#pragma once

#include <memory>
#include <vector>

#include <boost/asio.hpp>

#include "config.hpp"
#include "server_session.hpp"

namespace rt
{

class mgr_arena;

class listener {
private:
   boost::asio::ip::tcp::acceptor acceptor;
   std::vector<std::unique_ptr<mgr_arena>> const& arenas;
   long long next = 0;

   void do_accept();
   void on_accept(boost::system::error_code ec);

public:
   listener( boost::asio::ip::tcp::endpoint const& endpoint
           , std::vector<std::unique_ptr<mgr_arena>> const& arenas_
           , boost::asio::io_context& ioc);
   void run();
   void shutdown() {acceptor.cancel();}
};

}

