#pragma once

#include <string>

#include "net.hpp"

namespace occase
{

std::string make_ntf_body(
   std::string const& msg_title,
   std::string const& msg_body,
   std::string const& fcm_token);

class ntf_session : public std::enable_shared_from_this<ntf_session> {
public:
   struct config {
      std::string fcm_port {"443"};
      std::string fcm_host = {"fcm.googleapis.com"};
      std::string fcm_target = {"/fcm/send"};

      // NOTE: that key= is not part of the token but is there to simplify
      // setting the headers. The final for is key=token
      std::string fcm_server_token
      {"key=AAAAKavk7EY:APA91bEtq36uuNhvGSHu8hEE-EKNr3hsgso7IvDOWCHIZ6h_8LXPLz45EC3gxHUPxKxf3254TBM1bNxBby_8xP4U0pnsRh4JjV4uo4tbdBe2sSNrzZWoqTgcCTqmk3fIn3ltiJp3HKx2"};

      int version = 11;

      auto is_fcm_valid() const noexcept
      {
         auto const r =
            std::empty(fcm_port) ||
            std::empty(fcm_host) ||
            std::empty(fcm_target) ||
            std::empty(fcm_server_token);

         return !r;
      }
   };

   // Timeout used for the ssl handshake and the read operation.
   static constexpr auto timeout = 30;

private:
   beast::ssl_stream<beast::tcp_stream> stream_;
   beast::flat_buffer buffer_;
   http::request<http::string_body> req_;
   http::response<http::string_body> res_;

   void on_connect(
      beast::error_code ec,
      tcp::resolver::results_type::endpoint_type);

   void on_handshake(beast::error_code ec);

   void on_write(
      beast::error_code ec,
      std::size_t bytes_transferred);

   void on_read(
      beast::error_code ec,
      std::size_t bytes_transferred);

   void on_shutdown(beast::error_code ec);

public:
   explicit ntf_session(net::io_context& ioc, ssl::context& ctx);

   void run(
      config const& cfg,
      tcp::resolver::results_type results,
      std::string body);
};

}

