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

struct arena {
   int id_;
   net::io_context ioc {1};
   net::signal_set signals_;
   worker worker_;
   stats_server stats_server_;
   std::thread thread_;

   arena(listener_cfg const& cfg, int i)
   : id_ {i}
   , signals_ {ioc, SIGINT, SIGTERM}
   , worker_ {cfg.worker, i, ioc}
   , stats_server_ {cfg.stats, worker_, i, ioc}
   , thread_ { std::thread {[this](){ run();}} }
   {
      auto handler = [this](auto const& ec, auto n)
         { on_signal(ec, n); };

      signals_.async_wait(handler);
   }

   void on_signal(boost::system::error_code const& ec, int n)
   {
      if (ec) {
         if (ec == net::error::operation_aborted) {
            // No signal occurred, the handler was canceled. We just
            // leave.
            return;
         }

         log( loglevel::crit
            , "W{0}/arena::on_signal: Unhandled error '{1}'"
            , id_, ec.message());

         return;
      }

      log( loglevel::notice
         , "W{0}/arena::on_signal: Signal {1} has been captured."
         , id_, n);

      worker_.shutdown();
      stats_server_.shutdown();
   }

   ~arena()
   {
      thread_.join();
   }

   void run() noexcept
   {
      try {
         ioc.run();
      } catch (std::exception const& e) {
         log( loglevel::notice
            , "W{0}/arena::run: {1}", id_, e.what());
      }
   }
};

listener::listener(listener_cfg const& cfg)
: signals {ioc, SIGINT, SIGTERM}
, acceptor {ioc, {net::ip::tcp::v4(), cfg.port}}
{
   log( loglevel::info, "Binding server to {}"
      , acceptor.local_endpoint());

   auto arena_gen = [&cfg, i = -1]() mutable
      { return std::make_shared<arena>(cfg, ++i); };

   std::generate_n( std::back_inserter(arenas)
                  , cfg.number_of_workers
                  , arena_gen);
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

   auto const n = next % std::size(arenas);
   acceptor.async_accept(arenas[n]->ioc, handler);
}

void listener::on_accept( boost::system::error_code ec
                        , net::ip::tcp::socket peer)
{
   if (ec) {
      if (ec == net::error::operation_aborted) {
         log(loglevel::info, "Stopping accepting connections");
         return;
      }

      log( loglevel::debug, "listener::on_accept: {0}", ec.message());
   } else {
      auto const n = next % std::size(arenas);
      std::make_shared< worker_session
                      >(std::move(peer), arenas[n]->worker_)->accept();
      ++next;
   }

   do_accept();
}

}

