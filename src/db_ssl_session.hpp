#pragma once

#include <memory>

#include "net.hpp"
#include "db_session.hpp"

namespace rt
{

template <class Session>
class worker;

class db_ssl_session
   : public db_session<db_ssl_session>
   , public std::enable_shared_from_this<db_ssl_session> {
public:
   using stream_type =
      websocket::stream<beast::ssl_stream<beast::tcp_stream>>;
   using worker_type = worker<db_ssl_session>;
   using arg_type = worker_type&;
   using psession_type = proxy_session<db_ssl_session>;

private:
   stream_type stream;
   worker<db_ssl_session>& w;
   std::shared_ptr<psession_type> psession;

   void on_handshake(beast::error_code ec)
   {
      if (ec)
          return fail(ec, "handshake");

      accept();
   }

public:
   explicit
   db_ssl_session(tcp::socket&& stream, arg_type w_, ssl::context& ctx)
   : stream(std::move(stream), ctx)
   , w {w_}
   { }

   stream_type& ws() { return stream; }
   worker_type& db() { return w; }

   std::weak_ptr<psession_type> get_proxy_session(bool make_new_session)
   {
      if (!psession || make_new_session) {
         psession = std::make_shared<psession_type>();
         psession->session = shared_from_this();
      }

      return psession;
   }

   void run()
   {
      beast::get_lowest_layer(stream)
         .expires_after(std::chrono::seconds(30));

      auto self = shared_from_this();

      auto f = [self](auto ec)
         { self->on_handshake(ec); };

      stream.next_layer().async_handshake(ssl::stream_base::server,f);
   }
};

}

