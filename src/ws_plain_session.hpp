#pragma once

#include <memory>

#include "net.hpp"
#include "ws_session_impl.hpp"
#include "http_plain_session.hpp"

namespace occase
{

template <class Session>
class db_worker;

class ws_plain_session
   : public ws_session_impl<ws_plain_session>
   , public std::enable_shared_from_this<ws_plain_session> {
public:
   using stream_type = beast::tcp_stream;
   using ws_stream_type = websocket::stream<stream_type>;
   using worker_type = db_worker<stream_type>;
   using arg_type = worker_type&;

private:
   ws_stream_type stream_;
   worker_type& w;

public:
   explicit
   ws_plain_session(beast::tcp_stream&& stream, arg_type w_)
   : stream_(std::move(stream))
   , w {w_}
   { }

   ws_stream_type& ws() { return stream_; }
   worker_type& db() { return w; }
   worker_type const& db() const { return w; }
};

}

