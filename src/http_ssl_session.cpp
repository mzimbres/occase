#include "http_ssl_session.hpp"

namespace occase
{

http_ssl_session::http_ssl_session(
   tcp::socket&& stream,
   ssl::context& ctx,
   worker& w,
   beast::flat_buffer buffer)
: http_session_impl<http_ssl_session>(w, std::move(buffer))
, stream_(std::move(stream), ctx)
{ }

void http_ssl_session::run(std::chrono::seconds s)
{
   beast::get_lowest_layer(stream_).expires_after(s);

   // Perform the SSL handshake. Note, this is the buffered version
   // of the handshake.
   stream_.async_handshake(
       ssl::stream_base::server,
       this->buffer_.data(),
       beast::bind_front_handler(
	   &http_ssl_session::on_handshake,
	   this->shared_from_this()));
}

void http_ssl_session::do_eof(std::chrono::seconds ssl_timeout)
{
   beast::get_lowest_layer(stream_).expires_after(ssl_timeout);

   // Perform the SSL shutdown
   stream_.async_shutdown(
       beast::bind_front_handler(
	   &http_ssl_session::on_shutdown,
	   this->shared_from_this()));
}

void http_ssl_session::on_handshake(
   beast::error_code ec,
   std::size_t bytes_used)
{
   if (ec) {
      log::write( log::level::info
		, "http_ssl_session::on_handshake: {0}"
		, ec.message());
       return;
   }

   // Consume the portion of the buffer used by the handshake
   this->buffer_.consume(bytes_used);
   this->start();
}

void http_ssl_session::on_shutdown(beast::error_code ec)
{
   if (ec) {
      log::write( log::level::debug
		, "http_ssl_session::on_shutdown: {0}"
		, ec.message());
   }
}

} // occase
