#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <fstream>
#include <memory>
#include <string>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>

#include "config.hpp"
#include "crypto.hpp"
#include "logger.hpp"

namespace rt
{

beast::string_view mime_type(beast::string_view path)
{
    using beast::iequals;
    auto const ext = [&path]
    {
        auto const pos = path.rfind(".");
        if(pos == beast::string_view::npos)
            return beast::string_view{};
        return path.substr(pos);
    }();
    if(iequals(ext, ".htm"))  return "text/html";
    if(iequals(ext, ".html")) return "text/html";
    if(iequals(ext, ".php"))  return "text/html";
    if(iequals(ext, ".css"))  return "text/css";
    if(iequals(ext, ".txt"))  return "text/plain";
    if(iequals(ext, ".js"))   return "application/javascript";
    if(iequals(ext, ".json")) return "application/json";
    if(iequals(ext, ".xml"))  return "application/xml";
    if(iequals(ext, ".swf"))  return "application/x-shockwave-flash";
    if(iequals(ext, ".flv"))  return "video/x-flv";
    if(iequals(ext, ".png"))  return "image/png";
    if(iequals(ext, ".jpe"))  return "image/jpeg";
    if(iequals(ext, ".jpeg")) return "image/jpeg";
    if(iequals(ext, ".jpg"))  return "image/jpeg";
    if(iequals(ext, ".gif"))  return "image/gif";
    if(iequals(ext, ".bmp"))  return "image/bmp";
    if(iequals(ext, ".ico"))  return "image/vnd.microsoft.icon";
    if(iequals(ext, ".tiff")) return "image/tiff";
    if(iequals(ext, ".tif"))  return "image/tiff";
    if(iequals(ext, ".svg"))  return "image/svg+xml";
    if(iequals(ext, ".svgz")) return "image/svg+xml";
    return "application/text";
}

class http_conn : public std::enable_shared_from_this<http_conn> {
public:
   http_conn(tcp::socket socket, std::string docroot)
   : socket(std::move(socket))
   , deadline {socket.get_executor(), std::chrono::seconds(60)}
   , doc_root {std::move(docroot)}
   { }

   void run()
   {
      auto self = shared_from_this();
      auto f = [self](auto ec, auto n)
         { self->on_read_header(ec, n); };

      http::async_read_header(socket, buffer, header_parser, f);

      check_deadline();
   }

private:
   using file_body_type = http::file_body;
   using req_body_parser_type = http::request_parser<file_body_type>;
   using req_header_parser_type = http::request_parser<http::empty_body>;

   using resp_body_type = http::response<file_body_type>;

   tcp::socket socket;
   beast::flat_buffer buffer {8192};

   // We first read the header. This is needed on post to know in
   // advance the file name.
   req_header_parser_type header_parser;

   // We have to use a std::unique_ptr here while beast does not offer
   // correct move semantics for req_body_parser_type
   std::unique_ptr<req_body_parser_type> body_parser;

   http::response<http::dynamic_body> resp;

   // We we receive a get request we need a file_body response.
   std::unique_ptr<resp_body_type> file_body_resp;
   
   net::basic_waitable_timer<std::chrono::steady_clock> deadline;
   std::string doc_root;

   void on_read_body(boost::system::error_code ec, std::size_t n)
   {
      if (ec) {
         // TODO: Use the correct code.
         resp.result(http::status::not_found);
         resp.set(http::field::content_type, "text/plain");
         beast::ostream(resp.body()) << "File not found\r\n";
         std::cout << "Error: " << ec.message() << std::endl;
         std::cout << "docroot: " << doc_root << std::endl;
      } else {
         resp.result(http::status::ok);
         resp.set(http::field::server, "Beast");
      }

      write_response();
   }

   void on_read_header(boost::system::error_code ec, std::size_t n)
   {
      resp.version(header_parser.get().version());
      resp.keep_alive(false);

      switch (header_parser.get().method()) {
      case http::verb::post:
      {
         if (header_parser.get().target() == "/image") {
            std::string filename;
            auto const iter = header_parser.get().find("filename");
            if (iter != std::end(header_parser.get()))
               filename = iter->value().data();

            auto const pos = filename.find('.');
            if (pos == std::string::npos)
               return;

            auto const tmp = filename.substr(0, pos);

            // Workaround while req_body_parser_type does not offer a move
            // operator=
            body_parser =
               std::make_unique<req_body_parser_type>(std::move(header_parser));

            beast::error_code ec;
            auto const path = doc_root + "/" + tmp + ".jpg";
            std::cout << "Path: " << path << std::endl;
            body_parser->get().body().open( path.data()
                                          , beast::file_mode::write, ec);

            if (ec) { // TODO: Handle this.
               std::cout << ec.message() << std::endl;
            }

            auto self = shared_from_this();
            auto f = [self](auto ec, auto n)
               { self->on_read_body(ec, n); };

            http::async_read(socket, buffer, *body_parser, f);
         } else {
            resp.result(http::status::not_found);
            resp.set(http::field::content_type, "text/plain");
            beast::ostream(resp.body()) << "File not found\r\n";
            write_response();
         }
      }
      break;
      case http::verb::get:
      {
         std::cout << "Got a get req" << std::endl;
         std::string const filename = header_parser.get().target().data();

         if (std::empty(filename)) {
         } else {
            beast::error_code ec;
            auto const path = doc_root + filename;
            std::cout << "Path get: " << path << std::endl;
            http::file_body::value_type body;
            body.open(path.data(), beast::file_mode::scan, ec);

            if (ec) { // TODO: Handle this.
               std::cout << ec.message() << std::endl;
               return;
            }

            auto const size = std::size(body);
            file_body_resp = std::make_unique<resp_body_type>(
               std::piecewise_construct,
               std::make_tuple(std::move(body)),
               std::make_tuple(http::status::ok,
                  header_parser.get().version())
            );

            file_body_resp->set(http::field::server, BOOST_BEAST_VERSION_STRING);
            file_body_resp->set(http::field::content_type, mime_type(path));
            file_body_resp->content_length(size);
            file_body_resp->keep_alive(header_parser.keep_alive());

            auto self = shared_from_this();

            auto f = [self](auto ec, auto)
            {
                self->socket.shutdown(tcp::socket::shutdown_send, ec);
                self->deadline.cancel();
            };

            http::async_write(socket, *file_body_resp, f);
         }
      }
      break;
      default:
      {
         resp.result(http::status::bad_request);
         resp.set(http::field::content_type, "text/plain");
         write_response();
      }
      break;
      }

   }

   void write_response()
   {
      resp.set(http::field::content_length, resp.body().size());

      auto self = shared_from_this();

      auto f = [self](auto ec, auto)
      {
          self->socket.shutdown(tcp::socket::shutdown_send, ec);
          self->deadline.cancel();
      };

      http::async_write(socket, resp, f);
   }

   void check_deadline()
   {
      auto self = shared_from_this();

      auto f = [self](auto ec)
      {
         if (!ec)
             self->socket.close(ec);
      };

      deadline.async_wait(f);
   }
};

class listener {
private:
   net::io_context ioc {1};
   tcp::acceptor acceptor;
   std::string doc_root;

public:
   listener(unsigned short port, std::string docroot)
   : acceptor {ioc, {net::ip::tcp::v4(), port}}
   , doc_root {std::move(docroot)}
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
         std::make_shared<http_conn>(std::move(socket), doc_root)->run();
      }

      do_accept();
   }

   void shutdown()
   {
      acceptor.cancel();
   }
};

}

struct server_cfg {
   bool help;
   unsigned short port;
   std::string doc_root;
};

namespace po = boost::program_options;

auto get_cfg(int argc, char* argv[])
{
   server_cfg cfg;
   std::string conf_file;

   po::options_description desc("Options");
   desc.add_options()
   ("help,h"
   , "Produces help message")

   ("doc-root"
   , po::value<std::string>(&cfg.doc_root)->default_value("/tmp/imgs")
   , "Directory where image will be written to and read from.")

   ("config"
   , po::value<std::string>(&conf_file)
   , "The file containing the configuration.")

   ( "port"
   , po::value<unsigned short>(&cfg.port)->default_value(8888)
   , "Server listening port.")
   ;

   po::positional_options_description pos;
   pos.add("config", -1);

   po::variables_map vm;        
   po::store(po::command_line_parser(argc, argv).
         options(desc).positional(pos).run(), vm);
   po::notify(vm);    

   if (!std::empty(conf_file)) {
      std::ifstream ifs {conf_file};
      if (ifs) {
         po::store(po::parse_config_file(ifs, desc, true), vm);
         notify(vm);
      }
   }

   if (vm.count("help")) {
      std::cout << desc << "\n";
      return server_cfg {true};
   }

   return cfg;
}

using namespace rt;

int main(int argc, char* argv[])
{
   try {
      auto const cfg = get_cfg(argc, argv);
      if (cfg.help)
         return 0;

      init_libsodium();

      listener lst {cfg.port, cfg.doc_root};
      lst.run();
   } catch(std::exception const& e) {
      log(loglevel::notice, e.what());
      log(loglevel::notice, "Exiting with status 1 ...");
      return 1;
   }
}

