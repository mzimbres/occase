#pragma once

#include <memory>

#include "net.hpp"
#include "logger.hpp"
#include "ws_session_impl.hpp"
#include "http_ssl_session.hpp"

namespace occase
{

template <class Session>
class db_worker;

class ws_ssl_session
   : public ws_session_impl<ws_ssl_session>
   , public std::enable_shared_from_this<ws_ssl_session> {
public:
   using stream_type = beast::ssl_stream<beast::tcp_stream>;
   using ws_stream_type = websocket::stream<stream_type>;
   using worker_type = db_worker<beast::ssl_stream<beast::tcp_stream>>;
   using arg_type = worker_type&;

private:
   ws_stream_type stream_;
   worker_type& w;

public:
   explicit
   ws_ssl_session(beast::ssl_stream<beast::tcp_stream>&& stream, arg_type w_)
   : stream_(std::move(stream))
   , w {w_}
   { }

   ws_stream_type& ws() { return stream_; }
   worker_type& db() { return w; }
   worker_type const& db() const { return w; }
};

}

