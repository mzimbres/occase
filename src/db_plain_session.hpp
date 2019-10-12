#pragma once

#include <memory>

#include "net.hpp"
#include "db_session.hpp"

namespace rt
{

template <class Session>
class worker;

class db_plain_session;

struct proxy_session {
   // We have to use a weak pointer here. A share_ptr doen't work.
   // Even when the proxy_session object is explicitly killed. The
   // object is itself killed only after the last weak_ptr is
   // destructed. The shared_ptr will live that long in that case.
   std::weak_ptr<db_plain_session> session;
};

class db_plain_session
   : public db_session<db_plain_session>
   , public std::enable_shared_from_this<db_plain_session> {
public:
   using stream_type = websocket::stream<beast::tcp_stream>;
   using worker_type = worker<db_plain_session>;
   using arg_type = worker_type&;

private:
   stream_type stream;
   worker<db_plain_session>& w;
   std::shared_ptr<proxy_session> psession;

public:
   explicit db_plain_session(tcp::socket&& stream, arg_type w_)
   : stream(std::move(stream))
   , w {w_}
   { }

   stream_type& ws() { return stream; }
   worker_type& db() { return w; }

   std::weak_ptr<proxy_session> get_proxy_session(bool make_new_session)
   {
      if (!psession || make_new_session) {
         psession = std::make_shared<proxy_session>();
         psession->session = shared_from_this();
      }

      return psession;
   }
};

}

