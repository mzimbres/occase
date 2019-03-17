#pragma once

#include <memory>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>

#include "config.hpp"
#include "server_session.hpp"

namespace rt
{

class server_mgr;

class listener {
private:
   net::io_context ioc {1};
   net::signal_set signals;
   net::ip::tcp::acceptor acceptor;
   std::vector<std::shared_ptr<server_mgr>> const& workers;
   long long next = 0;

   void do_accept();
   void on_accept( boost::system::error_code ec
                 , net::ip::tcp::socket peer);

public:
   listener( net::ip::tcp::endpoint const& endpoint
           , std::vector<std::shared_ptr<server_mgr>> const& workers);
   void run();
   void shutdown() {acceptor.cancel();}
};

}

