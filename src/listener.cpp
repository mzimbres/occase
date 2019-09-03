#include "listener.hpp"

#include "logger.hpp"
#include "worker.hpp"

namespace rt
{

listener::listener(listener_cfg const& cfg)
: signals {ioc, SIGINT, SIGTERM}
, worker_ {cfg.worker, ioc}
, sserver {cfg.stats, ioc}
{ }

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

   sserver.shutdown();
   worker_.shutdown();
}

void listener::run()
{
   auto const handler = [this](auto const& ec, auto n)
      { on_signal(ec, n); };

   signals.async_wait(handler);

   sserver.run(worker_);

   ioc.run();
}

}

