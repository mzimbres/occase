#include "listener.hpp"

#include <iostream>
#include <algorithm>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include "logger.hpp"
#include "worker.hpp"
#include "worker_session.hpp"
#include "stats_server.hpp"

namespace rt
{

listener::listener(listener_cfg const& cfg)
: signals {ioc, SIGINT, SIGTERM}
, acceptor {ioc, {net::ip::tcp::v4(), cfg.port}}
{
   log( loglevel::info, "Binding server to {}"
      , acceptor.local_endpoint());

   auto arena_gen = [&cfg, i = -1]() mutable
      { return std::make_shared<worker_arena>(cfg.worker, ++i); };

   std::generate_n( std::back_inserter(warenas)
                  , cfg.number_of_workers
                  , arena_gen);

   sserver = std::make_unique<stats_server>(cfg.stats, warenas, ioc);
}

void listener::on_signal(boost::system::error_code const& ec, int n)
{
   if (ec) {
      if (ec == net::error::operation_aborted) {
         // No signal occurred, the handler was canceled. We just
         // leave.
         return;
      }

      log( loglevel::crit
         , "listener::on_signal: Unhandled error '{0}'"
         , ec.message());

      return;
   }

   log( loglevel::notice
      , "Signal {0} has been captured. "
        " Stopping listening for new connections"
      , n);

   acceptor.cancel();
   sserver->shutdown();
}

void listener::run()
{
   auto const handler = [this](auto const& ec, auto n)
      { on_signal(ec, n); };

   signals.async_wait(handler);

   if (!acceptor.is_open())
      return;

   do_accept();

   ioc.run();
}

void listener::do_accept()
{
   auto handler = [this](auto const& ec, auto socket)
      { on_accept(ec, std::move(socket)); };

   auto const n = next % std::size(warenas);
   acceptor.async_accept(warenas[n]->ioc_, handler);
}

void listener::on_accept( boost::system::error_code ec
                        , net::ip::tcp::socket peer)
{
   if (ec) {
      if (ec == net::error::operation_aborted) {
         log(loglevel::info, "Stopping accepting connections");
         return;
      }

      log(loglevel::debug, "listener::on_accept: {0}", ec.message());
   } else {
      auto const n = next % std::size(warenas);
      std::make_shared< worker_session
                      >(std::move(peer), warenas[n]->worker_)->accept();
      ++next;
   }

   do_accept();
}

}

