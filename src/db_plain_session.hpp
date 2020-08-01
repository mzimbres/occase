#pragma once

#include <memory>

#include "net.hpp"
#include "db_session.hpp"
#include "db_adm_plain_session.hpp"

namespace occase
{

template <class Session>
class db_worker;

class db_plain_session
   : public db_session<db_plain_session>
   , public std::enable_shared_from_this<db_plain_session> {
public:
   using stream_type = websocket::stream<beast::tcp_stream>;
   using worker_type = db_worker<db_adm_plain_session>;
   using arg_type = worker_type&;

private:
   stream_type stream_;
   worker_type& w;

public:
   explicit
   db_plain_session(beast::tcp_stream&& stream, arg_type w_)
   : stream_(std::move(stream))
   , w {w_}
   { }

   stream_type& ws() { return stream_; }
   worker_type& db() { return w; }
   worker_type const& db() const { return w; }
};

}

