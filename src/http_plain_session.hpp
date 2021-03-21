#pragma once

#include "net.hpp"
#include "http_session_impl.hpp"

namespace occase
{

class worker;

class http_plain_session
   : public http_session_impl<http_plain_session>
   , public std::enable_shared_from_this<http_plain_session> {
private:
   tcp_stream stream_;

public:
   explicit
   http_plain_session(
      tcp::socket&& stream,
      worker& w,
      beast::flat_buffer buffer)
   : http_session_impl<http_plain_session>(w, std::move(buffer))
   , stream_(std::move(stream))
   { }

   void run(std::chrono::seconds s)
   {
      beast::get_lowest_layer(stream_).expires_after(s);
      start();
   }

   tcp_stream& stream() { return stream_; } 
   tcp_stream release_stream() { return std::move(stream_); } 

   static bool is_ssl() noexcept {return false;}
   void do_eof(std::chrono::seconds)
   {
      beast::error_code ec;
      stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
   }
};

}

