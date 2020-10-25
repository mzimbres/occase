#pragma once

#include "net.hpp"
#include "logger.hpp"
#include "ws_session_impl.hpp"

#include <sys/types.h>
#include <sys/socket.h>

namespace occase
{

template <class Stream>
class db_worker;

template <class Session>
class acceptor_mgr {
public:
   using stream_type = typename Session::stream_type;

private:
   net::ip::tcp::acceptor acceptor_;

   void do_accept(db_worker<stream_type>& w, ssl::context& ctx)
   {
      auto handler = [this, &w, &ctx](auto const& ec, auto socket)
         { on_accept(w, ctx, ec, std::move(socket)); };

      acceptor_.async_accept(handler);
   }

   void on_accept( db_worker<stream_type>& w
                 , ssl::context& ctx
                 , boost::system::error_code ec
                 , net::ip::tcp::socket peer)
   {
      if (ec) {
         if (ec == net::error::operation_aborted) {
            log::write(log::level::info, "Stopping accepting connections");
            return;
         }

         log::write(log::level::info, "listener::on_accept: {0}", ec.message());
      } else {
         auto const n = w.get_cfg().http_session_timeout;
         std::make_shared< Session
                         >( std::move(peer)
                          , w
                          , ctx)->run(std::chrono::seconds {n});
      }

      do_accept(w, ctx);
   }

public:
   acceptor_mgr(net::io_context& ioc)
   : acceptor_ {ioc}
   { }

   auto is_open() const noexcept
      { return acceptor_.is_open(); }

   void run( db_worker<stream_type>& w
           , ssl::context& ctx
           , unsigned short port
           , int max_listen_connections)
   {
      tcp::endpoint endpoint {tcp::v4(), port};
      acceptor_.open(endpoint.protocol());

      int one = 1;
      auto const ret =
         setsockopt( acceptor_.native_handle()
                   , SOL_SOCKET
                   , SO_REUSEPORT
                   , &one, sizeof(one));

      if (ret == -1) {
         log::write( log::level::err
                   , "Unable to set socket option SO_REUSEPORT: {0}"
                   , strerror(errno));
      }

      acceptor_.bind(endpoint);

      boost::system::error_code ec;
      acceptor_.listen(max_listen_connections, ec);

      if (ec) {
         log::write(log::level::info, "acceptor_mgr::run: {0}.", ec.message());
      } else {
         log::write( log::level::info, "acceptor_mgr:run: Listening on {}"
                   , acceptor_.local_endpoint());
         log::write( log::level::info, "acceptor_mgr:run: TCP backlog set to {}"
                   , max_listen_connections);

         do_accept(w, ctx);
      }
   }

   void shutdown()
   {
      if (acceptor_.is_open()) {
         boost::system::error_code ec;
         acceptor_.cancel(ec);
         if (ec) {
            log::write( log::level::info
                      , "acceptor_mgr::shutdown: {0}.", ec.message());
         }
      }
   }
};

}

