#include "net.hpp"

#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;       // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

void fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

class ntf_session : public std::enable_shared_from_this<ntf_session> {
public:
   struct config {
      std::string port {"443"};

      std::string fcm_host = 
      {"fcm.googleapis.com"};

      std::string fcm_target = 
      {"/fcm/send"};

      // NOTE: that key= is not part of the token but is there to simplify
      // setting the headers.
      std::string fcm_server_token
      {"key=AAAAKavk7EY:APA91bEtq36uuNhvGSHu8hEE-EKNr3hsgso7IvDOWCHIZ6h_8LXPLz45EC3gxHUPxKxf3254TBM1bNxBby_8xP4U0pnsRh4JjV4uo4tbdBe2sSNrzZWoqTgcCTqmk3fIn3ltiJp3HKx2"};

      int version = 11;
   };

private:
   tcp::resolver resolver_;
   beast::ssl_stream<beast::tcp_stream> stream_;
   beast::flat_buffer buffer_;
   http::request<http::string_body> req_;
   http::response<http::string_body> res_;

public:
   explicit ntf_session(net::io_context& ioc, ssl::context& ctx)
   : resolver_ {ioc}
   , stream_ {ioc, ctx}
   {
   }

   void run(config const& cfg, std::string body)
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

      boost::system::error_code ec;
      auto res = resolver_.resolve(cfg.fcm_host, cfg.port, ec);
      on_resolve(ec, res);
   }

   void on_resolve(
      beast::error_code ec,
      tcp::resolver::results_type results)
   {
      if (ec)
          return fail(ec, "resolve");

      beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

      beast::get_lowest_layer(stream_).async_connect(
          results,
          beast::bind_front_handler(
              &ntf_session::on_connect,
              shared_from_this()));
   }

   void
   on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type)
   {
     if(ec)
         return fail(ec, "connect");

     // Perform the SSL handshake
     stream_.async_handshake(
         ssl::stream_base::client,
         beast::bind_front_handler(
             &ntf_session::on_handshake,
             shared_from_this()));
   }

   void
   on_handshake(beast::error_code ec)
   {
     if(ec)
         return fail(ec, "handshake");

     // Set a timeout on the operation
     beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

     // Send the HTTP request to the remote host
     http::async_write(stream_, req_,
         beast::bind_front_handler(
             &ntf_session::on_write,
             shared_from_this()));
   }

   void
   on_write(
     beast::error_code ec,
     std::size_t bytes_transferred)
   {
     boost::ignore_unused(bytes_transferred);

     if(ec)
         return fail(ec, "write");

     // Receive the HTTP response
     http::async_read(stream_, buffer_, res_,
         beast::bind_front_handler(
             &ntf_session::on_read,
             shared_from_this()));
   }

   void
   on_read(
     beast::error_code ec,
     std::size_t bytes_transferred)
   {
     boost::ignore_unused(bytes_transferred);

     if (ec)
         return fail(ec, "read");

     // Write the message to standard out
     std::cout << res_ << std::endl;

     // Set a timeout on the operation
     beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

     // Gracefully close the stream
     stream_.async_shutdown(
         beast::bind_front_handler(
             &ntf_session::on_shutdown,
             shared_from_this()));
   }

   void
   on_shutdown(beast::error_code ec)
   {
     if(ec == net::error::eof)
     {
         // Rationale:
         // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
         ec = {};
     }
     if(ec)
         return fail(ec, "shutdown");

     // If we get here then the connection is closed gracefully
   }
};

int main(int argc, char** argv)
{
   ntf_session::config cfg;

   std::string fcm_user_token = 
   {"eWyJqxRvo4M:APA91bE1pGVkhDramaP90PXVXqrRSRDPJ26LfcuzV292ouxNjbRcdGkPjC2D3tR6tpz7wCI9h94E6EdJ8Glqs-5Xqrpipp7EGex-GW2oh-oaMN3bOFZ45B1DkhwLBrrf3s9_JRE3j_ko"};
   //{"cx4hP-MTj-o:APA91bGhLMfbIZYTF2dlFFlJ-2JqycfBN8p5jDXx072g-JGQPWicuM9XMSHoME5Oh30fXqS7noZ7Ki-N0zWvj-KzYdDvlOpU5NPDkLEadhjjkW4JWVmxuCHFLI8kkyRGI34lKLGV3fKD"};

   json j;
   j["notification"]["body"] = "Nova mensagem.";
   j["notification"]["title"] = "Você tem uma nova mensagem.";
   j["priority"] = "high";
   j["data"]["click_action"] = "FLUTTER_NOTIFICATION_CLICK";
   j["data"]["id"] = "1";
   j["data"]["status"] = "done";
   j["to"] = fcm_user_token;

   std::cout << j.dump() << std::endl;

   std::cout
   << "--------------------------------"
   << std::endl;

   net::io_context ioc;
   ssl::context ctx{ssl::context::tlsv12_client};
   ctx.set_verify_mode(ssl::verify_none);
   std::make_shared<ntf_session>(ioc, ctx)->run(cfg, j.dump());
   ioc.run();

   return EXIT_SUCCESS;
}

