#include "acceptor_mgr.hpp"

#include "logger.hpp"
#include "worker.hpp"
#include "worker_session.hpp"

#include <sys/types.h>
#include <sys/socket.h>

namespace rt
{

acceptor_mgr::acceptor_mgr( unsigned short port
                          , net::io_context& ioc)
: acceptor {ioc}
{
   tcp::endpoint endpoint {tcp::v4(), port};
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
}

void acceptor_mgr::shutdown()
{
   acceptor.cancel();
}

void acceptor_mgr::run(worker& w)
{
   if (!acceptor.is_open()) {
      log(loglevel::crit, "acceptor_mgr::run: acceptor not open.");
      return;
   }

   do_accept(w);
}

void acceptor_mgr::do_accept(worker& w)
{
   auto handler = [this, &w](auto const& ec, auto socket)
      { on_accept(w, ec, std::move(socket)); };

   acceptor.async_accept(handler);
}

void
acceptor_mgr::on_accept( worker& w
                       , boost::system::error_code ec
                       , net::ip::tcp::socket peer)
{
   if (ec) {
      if (ec == net::error::operation_aborted) {
         log(loglevel::info, "Stopping accepting connections");
         return;
      }

      log(loglevel::debug, "listener::on_accept: {0}", ec.message());
   } else {
      std::make_shared<worker_session>(std::move(peer), w)->accept();
   }

   do_accept(w);
}

}


