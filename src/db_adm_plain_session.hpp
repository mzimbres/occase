#pragma once

#include "net.hpp"
#include "db_adm_session.hpp"

namespace rt
{

template <class WebSocketSession>
class db_worker;

template <class WebSocketSession>
class db_adm_plain_session
    : public db_adm_session< WebSocketSession
                           , db_adm_plain_session<WebSocketSession>>
    , public
      std::enable_shared_from_this<db_adm_plain_session<WebSocketSession>> {
public:
   using stream_type = beast::tcp_stream;
   using worker_type = db_worker<WebSocketSession>;
   using arg_type = worker_type&;

private:
   beast::tcp_stream stream_;
   arg_type w_;

public:
   explicit
   db_adm_plain_session(tcp::socket&& stream, arg_type w, ssl::context& ctx)
   : stream_(std::move(stream))
   , w_ {w}
   { }

   void run()
   {
      beast::get_lowest_layer(stream_)
         .expires_after(std::chrono::seconds(30));

      this->start();
   }

   stream_type& stream()
   {
      return stream_;
   }

   worker_type& db() { return w_; }

   void do_eof()
   {
      beast::error_code ec;
      stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
   }
};

}

