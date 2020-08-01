#pragma once

#include <memory>

#include "net.hpp"
#include "logger.hpp"
#include "db_session.hpp"
#include "db_adm_ssl_session.hpp"

namespace occase
{

template <class Session>
class db_worker;

class db_ssl_session
   : public db_session<db_ssl_session>
   , public std::enable_shared_from_this<db_ssl_session> {
public:
   using stream_type =
      websocket::stream<beast::ssl_stream<beast::tcp_stream>>;
   using worker_type = db_worker<db_adm_ssl_session>;
   using arg_type = worker_type&;

private:
   stream_type stream_;
   worker_type& w;

public:
   explicit
   db_ssl_session(beast::ssl_stream<beast::tcp_stream>&& stream, arg_type w_)
   : stream_(std::move(stream))
   , w {w_}
   { }

   stream_type& ws() { return stream_; }
   worker_type& db() { return w; }
   worker_type const& db() const { return w; }
};

}

