#pragma once

#include "net.hpp"
#include "logger.hpp"

#include <sys/types.h>
#include <sys/socket.h>

#include "http_plain_session.hpp"
#include "http_ssl_session.hpp"

namespace occase
{

template <class Stream>
struct make_session_type;

template <>
struct make_session_type<tcp_stream> {
  using type = http_plain_session;
};

template <>
struct make_session_type<ssl_stream> {
  using type = http_ssl_session;
};

template <class Stream>
class worker;

template <class Stream>
class acceptor_mgr {
public:
   using session_type = typename make_session_type<Stream>::type;

private:
   net::ip::tcp::acceptor acceptor_;

   void do_accept(worker<Stream>& w, ssl::context& ctx)
   {
      auto handler = [this, &w, &ctx](auto const& ec, auto socket)
         { on_accept(w, ctx, ec, std::move(socket)); };

      acceptor_.async_accept(handler);
   }

   void on_accept( worker<Stream>& w
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
         std::make_shared< session_type
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

   void run( worker<Stream>& w
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

