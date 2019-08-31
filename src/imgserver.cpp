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
#include "logger.hpp"

namespace rt
{

class http_conn : public std::enable_shared_from_this<http_conn> {
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

    void read_header()
    {
    }

private:
    tcp::socket socket_;
    beast::flat_buffer buffer_{8192};
    http::request_parser<http::empty_body> req0;
    http::request<http::file_body> request_;
    http::response<http::dynamic_body> response_;
    net::basic_waitable_timer<std::chrono::steady_clock> deadline_{
        socket_.get_executor(), std::chrono::seconds(60)};

   void read_request()
   {
      http::read_header(socket_, buffer_, req0);
      //-------------------------------------

      switch (req0.get().method()) {
         case http::verb::post:
         {
            //std::cout << req0.get().base()["filename"] << std::endl;
            auto const iter = req0.get().find("filename");
            if (iter != std::end(req0.get()))
                std::cout << "Image name: " << iter->value() << std::endl;

            http::request_parser<http::file_body> req1{std::move(req0)};
            beast::error_code ec;
            auto const path = "/tmp/jajaja.jpg";
            req1.get().body().open(path, beast::file_mode::write, ec);

            if (ec)
               std::cout << ec.message() << std::endl;

            auto self = shared_from_this();

            //auto f = [self](auto ec, auto bytes_transferred)
            //{
            //   std::ignore = bytes_transferred;

            //   if(!ec)
            //      self->process_request();
            //};

            http::read(socket_, buffer_, req1);
            process_request();

            //// Finish reading the message
            //read(stream, buffer, req1);

            //// Call the handler. It can take ownership
            //// if desired, since we are calling release()
            //handler(req1.release());
            break;

         }
         default:
         {
         }
      }
      //-------------------------------------

   }

    // Determine what needs to be done with the request message.
   void process_request()
   {
      response_.version(request_.version());
      response_.keep_alive(false);

      switch (request_.method()) {
      case http::verb::post:
      {
         std::cout << "====> I have received a post." << std::endl;
         response_.result(http::status::ok);
         response_.set(http::field::server, "Beast");
         create_response();
      }
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
      if (request_.target() == "/image") {
         response_.set(http::field::content_type, "text/html");
         beast::ostream(response_.body()) << "";
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

class listener {
private:
   net::io_context ioc {1};
   tcp::acceptor acceptor;

public:
   listener(unsigned short port)
   : acceptor {ioc, {net::ip::tcp::v4(), port}}
   {}

   void run()
   {
      do_accept();
      ioc.run();
   }

   void do_accept()
   {
      auto handler = [this](auto const& ec, auto socket)
         { on_accept(ec, std::move(socket)); };

      acceptor.async_accept(ioc, handler);
   }

   void on_accept( boost::system::error_code ec
                 , net::ip::tcp::socket socket)
   {
      if (ec) {
         if (ec == net::error::operation_aborted) {
            log( loglevel::info
               , "imgserver: Stopping accepting connections");
            return;
         }

         log(loglevel::debug, "imgserver: on_accept: {1}", ec.message());
      } else {
         std::make_shared<http_conn>(std::move(socket))->start();
      }

      do_accept();
   }

   void shutdown()
   {
      acceptor.cancel();
   }
};

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

      auto const port = static_cast<unsigned short>(std::atoi(argv[1]));
      listener lst {port};
      lst.run();
   } catch(std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
      return EXIT_FAILURE;
   }
}

