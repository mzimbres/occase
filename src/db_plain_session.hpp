#pragma once

#include <memory>

#include "net.hpp"
#include "db_session.hpp"
#include "db_adm_plain_session.hpp"

namespace rt
{

template <class Session>
class db_worker;

class db_plain_session
   : public db_session<db_plain_session>
   , public std::enable_shared_from_this<db_plain_session> {
public:
   using stream_type = websocket::stream<beast::tcp_stream>;
   using worker_type = db_worker<db_plain_session>;
   using arg_type = worker_type&;
   using psession_type = proxy_session<db_plain_session>;
   using db_adm_session_type = db_adm_plain_session<db_plain_session>;

private:
   stream_type stream_;
   arg_type w;
   std::shared_ptr<psession_type> psession;

public:
   explicit
   db_plain_session(beast::tcp_stream&& stream, arg_type w_)
   : stream_(std::move(stream))
   , w {w_}
   { }

   stream_type& ws() { return stream_; }
   worker_type& db() { return w; }

   std::weak_ptr<psession_type> get_proxy_session(bool make_new_session)
   {
      if (!psession || make_new_session) {
         psession = std::make_shared<psession_type>();
         psession->session = shared_from_this();
      }

      return psession;
   }
};

}

