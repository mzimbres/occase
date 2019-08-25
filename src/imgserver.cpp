#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <string>

#include "config.hpp"
#include "crypto.hpp"

namespace rt
{

class http_conn : public std::enable_shared_from_this<http_conn>
{
public:
    http_conn(tcp::socket socket)
    : socket_(std::move(socket))
    { }

    // Initiate the asynchronous operations associated with the connection.
    void start()
    {
        read_request();
        check_deadline();
    }

private:
    tcp::socket socket_;
    beast::flat_buffer buffer_{8192};
    http::request<http::dynamic_body> request_;
    http::response<http::dynamic_body> response_;
    net::basic_waitable_timer<std::chrono::steady_clock> deadline_{
        socket_.get_executor(), std::chrono::seconds(60)};

   void read_request()
   {
      auto self = shared_from_this();

      auto f = [self](auto ec, auto bytes_transferred)
      {
         std::ignore = bytes_transferred;

         if(!ec)
            self->process_request();
      };

      http::async_read(socket_, buffer_, request_, f);
   }

    // Determine what needs to be done with the request message.
   void process_request()
   {
      response_.version(request_.version());
      response_.keep_alive(false);

      switch (request_.method()) {
      case http::verb::post:
         std::cout << "====> I have received a post." << std::endl;
         response_.result(http::status::ok);
         response_.set(http::field::server, "Beast");
         create_response();
      break;

      default:
         // We return responses indicating an error if
         // we do not recognize the request method.
         response_.result(http::status::bad_request);
         response_.set(http::field::content_type, "text/plain");
         beast::ostream(response_.body())
         << "Invalid request-method '"
         << std::string(request_.method_string())
         << "'";
      break;
      }

      write_response();
   }

   void create_response()
   {
      // Later we will generate a filename send it to S3 and return
      // the filename to the app.
      constexpr auto* url = "https://avatarfiles.alphacoders.com/116/116803.jpg";

      if (request_.target() == "/image") {
         response_.set(http::field::content_type, "text/html");
         beast::ostream(response_.body()) << url;
      } else if (request_.target() == "/expiration") {
         response_.set(http::field::content_type, "text/html");
         beast::ostream(response_.body()) << "";
      } else {
         response_.result(http::status::not_found);
         response_.set(http::field::content_type, "text/plain");
         beast::ostream(response_.body()) << "File not found\r\n";
      }
   }

    void write_response()
    {
        auto self = shared_from_this();

        response_.set(http::field::content_length, response_.body().size());

        http::async_write(
            socket_,
            response_,
            [self](beast::error_code ec, std::size_t)
            {
                self->socket_.shutdown(tcp::socket::shutdown_send, ec);
                self->deadline_.cancel();
            });
    }

    void check_deadline()
    {
        auto self = shared_from_this();

        deadline_.async_wait(
            [self](beast::error_code ec)
            {
                if(!ec)
                {
                    // Close socket to cancel any outstanding operation.
                    self->socket_.close(ec);
                }
            });
    }
};

void http_server(tcp::acceptor& acceptor, tcp::socket& socket)
{
   auto f = [&](beast::error_code ec)
   {
      if (!ec)
         std::make_shared<http_conn>(std::move(socket))->start();

      http_server(acceptor, socket);
   };

   std::cout << "====> Accepting." << std::endl;

   acceptor.async_accept(socket, f);
}

}

int main(int argc, char* argv[])
{
   using namespace rt;

   try {
      if (argc != 2) {
         std::cerr << "Usage: " << argv[0] << " <port>\n";
         std::cerr << "  For IPv4, try:\n";
         std::cerr << "    receiver 80\n";
         std::cerr << "  For IPv6, try:\n";
         std::cerr << "    receiver 80\n";
         return EXIT_FAILURE;
      }

      init_libsodium();

      auto port = static_cast<unsigned short>(std::atoi(argv[1]));
      net::io_context ioc{1};
      tcp::acceptor acceptor{ioc, {net::ip::tcp::v4(), port}};
      tcp::socket socket{ioc};
      http_server(acceptor, socket);
      ioc.run();
   } catch(std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
      return EXIT_FAILURE;
   }
}

