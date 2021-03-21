#include "acceptor_mgr.hpp"

#include <sys/types.h>
#include <sys/socket.h>

#include "net.hpp"
#include "logger.hpp"
#include "worker.hpp"
#include "http_plain_session.hpp"
#include "http_ssl_session.hpp"

namespace occase
{

class detect_session : public std::enable_shared_from_this<detect_session> {
private:
   beast::tcp_stream stream_;
   ssl::context& ctx_;
   beast::flat_buffer buffer_;
   worker& w_;

public:
   detect_session(tcp::socket&& socket, ssl::context& ctx, worker& w)
   : stream_(std::move(socket))
   , ctx_(ctx)
   , w_ {w}
   { }

   void run()
   {
      beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

      async_detect_ssl(
         stream_,
         buffer_,
         beast::bind_front_handler(
	    &detect_session::on_detect,
	    shared_from_this()));
   }

   void on_detect(beast::error_code ec, bool result)
   {
      if (ec) {
	 log::write(log::level::debug , "on_detect: {0}", ec.message());
         return;
      }

      auto const n = w_.get_cfg().http_session_timeout;
      std::chrono::seconds timeout{n};

      if (result) {
         std::make_shared<http_ssl_session>(
            stream_.release_socket(),
            ctx_,
            w_,
            std::move(buffer_))->run(timeout);

         return;
      }

      std::make_shared<http_plain_session>(
         stream_.release_socket(),
	 w_,
         std::move(buffer_))->run(timeout);
   }
};

void acceptor_mgr::do_accept(worker& w, ssl::context& ctx)
{
   auto handler = [this, &w, &ctx](auto const& ec, auto socket)
      { on_accept(w, ctx, ec, std::move(socket)); };

   acceptor_.async_accept(handler);
}

void acceptor_mgr::on_accept(
   worker& w,
   ssl::context& ctx,
   boost::system::error_code ec,
   tcp::socket peer)
{
   if (ec) {
      if (ec == net::error::operation_aborted) {
	 log::write(log::level::info, "Stopping accepting connections");
	 return;
      }

      log::write(log::level::info, "listener::on_accept: {0}", ec.message());
   } else {
      std::make_shared<detect_session>(
	  std::move(peer),
	  ctx,
	  w)->run();
   }

   do_accept(w, ctx);
}

acceptor_mgr::acceptor_mgr(net::io_context& ioc)
: acceptor_ {ioc}
{ }

void acceptor_mgr::run(
   worker& w,
   ssl::context& ctx,
   unsigned short port,
   int max_listen_connections)
{
   tcp::endpoint endpoint {tcp::v4(), port};
   acceptor_.open(endpoint.protocol());

   int one = 1;
   auto const ret =
      setsockopt( acceptor_.native_handle()
		, SOL_SOCKET
		, SO_REUSEPORT
		, &one, sizeof(one));

   if (ret == -1) {
      log::write( log::level::err
		, "Unable to set socket option SO_REUSEPORT: {0}"
		, strerror(errno));
   }

   acceptor_.bind(endpoint);

   boost::system::error_code ec;
   acceptor_.listen(max_listen_connections, ec);

   if (ec) {
      log::write(log::level::info, "acceptor_mgr::run: {0}.", ec.message());
   } else {
      log::write( log::level::info, "acceptor_mgr:run: Listening on {}"
		, acceptor_.local_endpoint());
      log::write( log::level::info, "acceptor_mgr:run: TCP backlog set to {}"
		, max_listen_connections);

      do_accept(w, ctx);
   }
}

void acceptor_mgr::shutdown()
{
   if (acceptor_.is_open()) {
      boost::system::error_code ec;
      acceptor_.cancel(ec);
      if (ec) {
	 log::write( log::level::info
		   , "acceptor_mgr::shutdown: {0}.", ec.message());
      }
   }
}

} // occase
