#pragma once

#include "net.hpp"
#include "db_adm_session.hpp"

namespace rt
{

template <class AdmSession>
class db_worker;

class db_plain_session;

class db_adm_plain_session
   : public db_adm_session<db_adm_plain_session>
   , public std::enable_shared_from_this<db_adm_plain_session> {
public:
   using stream_type = beast::tcp_stream;
   using worker_type = db_worker<db_adm_plain_session>;
   using arg_type = worker_type&;
   using db_session_type = db_plain_session;

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
   
   stream_type release_stream()
   {
      return std::move(stream_);
   }

   worker_type& db() { return w_; }

   void do_eof()
   {
      beast::error_code ec;
      stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
   }
};

}

