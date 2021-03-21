#pragma once

#include "net.hpp"
#include "http_session_impl.hpp"

namespace occase
{

class worker;

class http_ssl_session
   : public http_session_impl<http_ssl_session>
   , public std::enable_shared_from_this<http_ssl_session> {
private:
   ssl_stream stream_;

   void on_handshake(beast::error_code ec, std::size_t bytes_used);
   void on_shutdown(beast::error_code ec);

public:
   explicit
   http_ssl_session(
      tcp::socket&& stream,
      ssl::context& ctx,
      worker& w,
      beast::flat_buffer buffer);

   void run(std::chrono::seconds s);
   ssl_stream& stream() { return stream_; }
   ssl_stream release_stream() { return std::move(stream_); }
   static bool is_ssl() noexcept {return true;}
   void do_eof(std::chrono::seconds ssl_timeout);
};

}

