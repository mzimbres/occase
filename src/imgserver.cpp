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

   void run()
   {
      auto self = shared_from_this();
      auto f = [self](auto ec, auto n)
         { self->on_read_header(ec, n); };

      http::async_read_header(socket_, buffer_, header_parser, f);

      check_deadline();
   }

private:
   using body_parser_type = http::request_parser<http::file_body>;

   tcp::socket socket_;
   beast::flat_buffer buffer_{8192};
   http::request_parser<http::empty_body> header_parser;
   http::request<http::file_body> request_;
   http::response<http::dynamic_body> response_;
   net::basic_waitable_timer<std::chrono::steady_clock> deadline_{
       socket_.get_executor(), std::chrono::seconds(60)};

   void on_read_header(boost::system::error_code ec, std::size_t n)
   {
      response_.version(request_.version());
      response_.keep_alive(false);

      switch (header_parser.get().method()) {
      case http::verb::post:
      {
         if (header_parser.get().target() == "/image") {
            std::string filename;
            auto const iter = header_parser.get().find("filename");
            if (iter != std::end(header_parser.get()))
               filename = iter->value().data();

            std::cout << "Image name: " << filename << std::endl;

            body_parser_type body_parser{std::move(header_parser)};

            beast::error_code ec;
            auto const path = "/tmp/" + filename;
            body_parser.get().body().open( path.c_str()
                                         , beast::file_mode::write, ec);

            if (ec) {
               std::cout << ec.message() << std::endl;
            }

            http::read(socket_, buffer_, body_parser);
            response_.result(http::status::ok);
            response_.set(http::field::server, "Beast");
         } else {
            response_.result(http::status::not_found);
            response_.set(http::field::content_type, "text/plain");
            beast::ostream(response_.body()) << "File not found\r\n";
         }
      }
      break;
      default:
      {
         response_.result(http::status::bad_request);
         response_.set(http::field::content_type, "text/plain");
      }
      break;
      }

      write_response();
   }

   void write_response()
   {
      response_.set(http::field::content_length, response_.body().size());

      auto self = shared_from_this();

      auto f = [self](auto ec, auto)
      {
          self->socket_.shutdown(tcp::socket::shutdown_send, ec);
          self->deadline_.cancel();
      };

      http::async_write(socket_, response_, f);
   }

   void check_deadline()
   {
       auto self = shared_from_this();

       auto f = [self](beast::error_code ec)
       {
           if (!ec)
               self->socket_.close(ec);
       };

       deadline_.async_wait(f);
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
         std::make_shared<http_conn>(std::move(socket))->run();
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

