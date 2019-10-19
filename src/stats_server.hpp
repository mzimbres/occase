#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <cstdlib>
#include <iostream>

#include "net.hpp"
#include "logger.hpp"
#include "worker.hpp"
#include "http_session.hpp"

namespace rt
{

struct stats_server_cfg {
   std::string port;
};

template <class Session>
class stats_server {
private:
   tcp::acceptor acceptor_;

   void do_accept(worker<Session> const& w)
   {
      auto handler = [this, &w](auto const& ec, auto socket)
         { on_accept(w, ec, std::move(socket)); };

      acceptor_.async_accept(acceptor_.get_executor(), handler);
   }

   void on_accept( worker<Session> const& w
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
         std::make_shared< http_session<Session>
                         >(std::move(socket), w)->start();
      }

      do_accept(w);
   }

public:
   stats_server(stats_server_cfg const& cfg, net::io_context& ioc)
   : acceptor_ {ioc, { net::ip::tcp::v4()
                     , static_cast<unsigned short>(std::stoi(cfg.port))}}
   { }

   void run(worker<Session> const& w)
   {
      do_accept(w);
   }

   void shutdown()
   {
      boost::system::error_code ec;
      acceptor_.cancel(ec);
      if (ec) {
         log( loglevel::info
            , "stats_server::shutdown: {0}.", ec.message());
      }
   }
};

}

