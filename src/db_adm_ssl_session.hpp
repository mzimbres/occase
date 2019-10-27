#pragma once

#include "net.hpp"
#include "db_adm_session.hpp"

namespace rt
{

template <class WebSocketSession>
class db_worker;

template <class WebSocketSession>
class db_adm_ssl_session
    : public db_adm_session< WebSocketSession
                           , db_adm_ssl_session<WebSocketSession>>
    , public
      std::enable_shared_from_this<db_adm_ssl_session<WebSocketSession>> {
public:
   using stream_type = beast::ssl_stream<beast::tcp_stream>;
   using worker_type = db_worker<WebSocketSession>;
   using arg_type = worker_type&;

private:
   stream_type stream_;
   arg_type w_;

   void on_handshake(beast::error_code ec, std::size_t bytes_used)
   {
       if (ec) {
          log( loglevel::info
             , "db_adm_ssl_session::on_handshake: {0}"
             , ec.message());
           return;
       }

       // Consume the portion of the buffer used by the handshake
       this->buffer_.consume(bytes_used);

       this->start();
   }

   void on_shutdown(beast::error_code ec)
   {
      if (ec) {
         log( loglevel::debug
            , "db_adm_ssl_session::on_shutdown: {0}"
            , ec.message());
      }
   }

public:
   explicit
   db_adm_ssl_session(tcp::socket&& stream, arg_type w, ssl::context& ctx)
   : stream_(std::move(stream), ctx)
   , w_ {w}
   { }

   void run()
   {
      beast::get_lowest_layer(stream_)
         .expires_after(std::chrono::seconds(30));

      // Perform the SSL handshake
      // Note, this is the buffered version of the handshake.
      stream_.async_handshake(
          ssl::stream_base::server,
          this->buffer_.data(),
          beast::bind_front_handler(
              &db_adm_ssl_session::on_handshake,
              this->shared_from_this()));
   }

   stream_type& stream()
   {
      return stream_;
   }

   worker_type& db() { return w_; }

   void do_eof()
   {
       beast::get_lowest_layer(stream_)
          .expires_after(std::chrono::seconds(30));

       // Perform the SSL shutdown
       stream_.async_shutdown(
           beast::bind_front_handler(
               &db_adm_ssl_session::on_shutdown,
               this->shared_from_this()));
   }
};

}

