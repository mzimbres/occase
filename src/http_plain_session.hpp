#pragma once

#include "net.hpp"
#include "http_session_impl.hpp"

namespace occase
{

template <class Stream>
class worker;

template <class Stream>
class ws_session;

class http_plain_session
   : public http_session_impl<http_plain_session>
   , public std::enable_shared_from_this<http_plain_session> {
public:
   using stream_type = tcp_stream;
   using worker_type = worker<stream_type>;
   using ws_session_type = ws_session<stream_type>;

private:
   stream_type stream_;
   worker_type& w_;

public:
   explicit
   http_plain_session(tcp::socket&& stream, worker_type& w, ssl::context& ctx)
   : stream_(std::move(stream))
   , w_ {w}
   { }

   void run(std::chrono::seconds s)
   {
      beast::get_lowest_layer(stream_).expires_after(s);
      start();
   }

   stream_type& stream() { return stream_; } 
   stream_type release_stream() { return std::move(stream_); } 
   worker_type& db() { return w_; }

   void do_eof(std::chrono::seconds)
   {
      beast::error_code ec;
      stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
   }
};

}

