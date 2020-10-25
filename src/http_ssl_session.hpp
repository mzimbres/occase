#pragma once

#include "net.hpp"
#include "http_session_impl.hpp"

namespace occase
{

template <class Stream>
class db_worker;

class ws_ssl_session;

class http_ssl_session
   : public http_session_impl<http_ssl_session>
   , public std::enable_shared_from_this<http_ssl_session> {
public:
   using stream_type = beast::ssl_stream<beast::tcp_stream>;
   using worker_type = db_worker<stream_type>;
   using arg_type = worker_type;
   using ws_session_type = ws_ssl_session;

private:
   stream_type stream_;
   arg_type& w_;

   void on_handshake(beast::error_code ec, std::size_t bytes_used)
   {
       if (ec) {
          log::write( log::level::info
                    , "http_ssl_session::on_handshake: {0}"
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
         log::write( log::level::debug
                   , "http_ssl_session::on_shutdown: {0}"
                   , ec.message());
      }
   }

public:
   explicit
   http_ssl_session(tcp::socket&& stream, arg_type& w, ssl::context& ctx)
   : stream_(std::move(stream), ctx)
   , w_ {w}
   { }

   void run(std::chrono::seconds s)
   {
      beast::get_lowest_layer(stream_).expires_after(s);

      // Perform the SSL handshake. Note, this is the buffered version
      // of the handshake.
      stream_.async_handshake(
          ssl::stream_base::server,
          this->buffer_.data(),
          beast::bind_front_handler(
              &http_ssl_session::on_handshake,
              this->shared_from_this()));
   }

   stream_type& stream() { return stream_; }
   stream_type release_stream() { return std::move(stream_); }
   worker_type& db() { return w_; }

   void do_eof(std::chrono::seconds ssl_timeout)
   {
       beast::get_lowest_layer(stream_).expires_after(ssl_timeout);

       // Perform the SSL shutdown
       stream_.async_shutdown(
           beast::bind_front_handler(
               &http_ssl_session::on_shutdown,
               this->shared_from_this()));
   }
};

}

