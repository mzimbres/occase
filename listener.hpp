#pragma once

#include <memory>
#include <vector>

#include <boost/asio.hpp>

#include "config.hpp"
#include "server_session.hpp"

namespace rt
{

class server_mgr;

class listener {
private:
   net::ip::tcp::acceptor acceptor;
   std::vector<std::shared_ptr<server_mgr>> const& workers;
   long long next = 0;

   void do_accept();
   void on_accept(boost::system::error_code ec);

public:
   listener( net::ip::tcp::endpoint const& endpoint
           , std::vector<std::shared_ptr<server_mgr>> const& arenas_
           , net::io_context& ioc);
   void run();
   void shutdown() {acceptor.cancel();}
};

}

