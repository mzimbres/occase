#include "listener.hpp"

#include <iostream>
#include <algorithm>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include "logger.hpp"
#include "worker.hpp"
#include "worker_session.hpp"
#include "stats_server.hpp"

#include <sys/types.h>
#include <sys/socket.h>

namespace rt
{

listener::listener(listener_cfg const& cfg)
: signals {ioc, SIGINT, SIGTERM}
, acceptor {ioc}
, worker_ {cfg.worker, ioc}
, sserver {cfg.stats, ioc}
{
   tcp::endpoint endpoint {tcp::v4(), cfg.port};
   acceptor.open(endpoint.protocol());

   int one = 1;
   auto const ret =
      setsockopt( acceptor.native_handle()
                , SOL_SOCKET
                , SO_REUSEPORT
                , &one, sizeof(one));

   if (ret == -1) {
      log( loglevel::err
         , "Unable to set socket option SO_REUSEPORT: {0}"
         , strerror(errno));
   }

   acceptor.bind(endpoint);
   acceptor.listen();

   log( loglevel::info, "Binding server to {}"
      , acceptor.local_endpoint());

   sserver.run(worker_);
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
   sserver.shutdown();
   worker_.shutdown();
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

   acceptor.async_accept(ioc, handler);
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
      std::make_shared<worker_session>(std::move(peer), worker_)->accept();
   }

   do_accept();
}

}

