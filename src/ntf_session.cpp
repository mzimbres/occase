#include "ntf_session.hpp"

#include <nlohmann/json.hpp>

#include "logger.hpp"

using json = nlohmann::json;

namespace occase
{

std::string
make_ntf_body( std::string const& msg_title
             , std::string const& msg_body
             , std::string const& fcm_user_token)
{
   json j;
   j["notification"]["title"] = msg_title;
   j["notification"]["body"] = msg_body;
   j["priority"] = "high";
   j["data"]["click_action"] = "FLUTTER_NOTIFICATION_CLICK";
   j["data"]["id"] = "1";
   j["data"]["status"] = "done";
   j["to"] = fcm_user_token;

   return j.dump();
}

ntf_session::ntf_session(net::io_context& ioc, ssl::context& ctx)
: stream_ {ioc, ctx}
{ }

void ntf_session::run(
   args const& cfg,
   tcp::resolver::results_type res,
   std::string body)
{
   // Set SNI Hostname (many hosts need this to handshake successfully)
   if (!SSL_set_tlsext_host_name(stream_.native_handle(), cfg.fcm_host.c_str())) {
      beast::error_code ec
      { static_cast<int>(::ERR_get_error())
      , net::error::get_ssl_category()};

      std::cerr << ec.message() << "\n";
      return;
   }

   req_.version(cfg.version);
   req_.method(http::verb::post);
   req_.target(cfg.fcm_target);

   req_.set(http::field::host, cfg.fcm_host);
   req_.set(http::field::content_type, "application/json");
   req_.set(http::field::authorization, cfg.fcm_server_token);
   req_.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
   req_.set(http::field::content_length, std::size(body));

   req_.body() = std::move(body);

   beast::get_lowest_layer(stream_).async_connect(
      res,
      beast::bind_front_handler(
         &ntf_session::on_connect,
         shared_from_this()));
}

void ntf_session::on_connect(
   beast::error_code ec,
   tcp::resolver::results_type::endpoint_type)
{
   if (ec) {
      log::write(log::level::debug, "on_connect: {0}", ec.message());
      return;
   }

   stream_.async_handshake(
      ssl::stream_base::client,
      beast::bind_front_handler(
         &ntf_session::on_handshake,
         shared_from_this()));
}

void ntf_session::on_handshake(beast::error_code ec)
{
   if (ec) {
      log::write(log::level::debug, "on_handshake: {0}", ec.message());
      return;
   }

   beast::get_lowest_layer(stream_)
      .expires_after(std::chrono::seconds(timeout));

   http::async_write(
      stream_,
      req_,
      beast::bind_front_handler(
         &ntf_session::on_write,
         shared_from_this()));
}

void ntf_session::on_write(
   beast::error_code ec,
   std::size_t bytes_transferred)
{
   boost::ignore_unused(bytes_transferred);

   if (ec) {
      log::write(log::level::debug, "on_write: {0}", ec.message());
      return;
   }

   http::async_read(
      stream_,
      buffer_,
      res_,
      beast::bind_front_handler(
         &ntf_session::on_read,
         shared_from_this()));
}

void ntf_session::on_read(
   beast::error_code ec,
   std::size_t bytes_transferred)
{
   boost::ignore_unused(bytes_transferred);

   if (ec) {
      log::write(log::level::debug, "on_read: {0}", ec.message());
      return;
   }

   if (res_.result() != beast::http::status::ok) {
      log::write(log::level::debug, "Response status: fail.");
      //std::cout << res_ << std::endl;
   }

   beast::get_lowest_layer(stream_)
      .expires_after(std::chrono::seconds(timeout));

   stream_.async_shutdown(
      beast::bind_front_handler(
         &ntf_session::on_shutdown,
         shared_from_this()));
}

void ntf_session::on_shutdown(beast::error_code ec)
{
   if (ec == net::error::eof) {
       // Rationale:
       // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
       ec = {};
   }

   if (ec) {
      log::write(log::level::debug, "on_shutdown: {0}", ec.message());
      return;
   }
}

}

