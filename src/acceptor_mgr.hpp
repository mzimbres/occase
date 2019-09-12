#pragma once

#include "net.hpp"
#include "logger.hpp"
#include "worker_session.hpp"

#include <sys/types.h>
#include <sys/socket.h>

namespace rt
{

template <class Session>
class acceptor_mgr {
private:
   using arg_type = typename Session::arg_type;
   net::ip::tcp::acceptor acceptor;

   void do_accept(arg_type w)
   {
      auto handler = [this, &w](auto const& ec, auto socket)
         { on_accept(w, ec, std::move(socket)); };

      acceptor.async_accept(handler);
   }

   void on_accept( arg_type w
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
         std::make_shared<Session>(std::move(peer), w)->accept();
      }

      do_accept(w);
   }

public:
   acceptor_mgr(net::io_context& ioc)
   : acceptor {ioc}
   { }

   void run( arg_type w
           , unsigned short port
           , int max_listen_connections)
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

      boost::system::error_code ec;
      acceptor.listen(max_listen_connections, ec);

      if (ec) {
         log(loglevel::debug, "acceptor_mgr::run: {0}.", ec.message());
      } else {
         log( loglevel::info, "acceptor_mgr:run: Listening on {}"
            , acceptor.local_endpoint());
         log( loglevel::info, "acceptor_mgr:run: TCP backlog set to {}"
            , max_listen_connections);

         do_accept(w);
      }
   }

   void shutdown()
   {
      acceptor.cancel();
   }
};

}

