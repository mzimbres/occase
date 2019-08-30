#include "stats_server.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <cstdlib>
#include <iostream>

#include "config.hpp"
#include "logger.hpp"
#include "worker.hpp"
#include "http_session.hpp"

namespace rt {

stats_server::stats_server(stats_server_cfg const& cfg, net::io_context& ioc)
: acceptor_ {ioc, { net::ip::tcp::v4()
                  , static_cast<unsigned short>(std::stoi(cfg.port))}}
{ }

void stats_server::on_accept( worker const& w
                            , boost::system::error_code ec
                            , net::ip::tcp::socket socket)
{
   if (ec) {
      if (ec == net::error::operation_aborted) {
         log( loglevel::info
            , "stats_server: Stopping accepting connections");
         return;
      }

      log(loglevel::debug, "stats: on_accept: {1}", ec.message());
   } else {
      std::make_shared<http_session>(std::move(socket), w)->start();
   }

   do_accept(w);
}

void stats_server::do_accept(worker const& w)
{
   auto handler = [this, &w](auto const& ec, auto socket)
      { on_accept(w, ec, std::move(socket)); };

   acceptor_.async_accept(acceptor_.get_executor(), handler);
}

void stats_server::run(worker const& w)
{
   do_accept(w);
}

void stats_server::shutdown()
{
   acceptor_.cancel();
}

}

