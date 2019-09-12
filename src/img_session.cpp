#include "img_session.hpp"

#include <iostream>

namespace rt
{

beast::string_view get_filename(beast::string_view path)
{
   auto const pos = path.rfind("/");
   if (pos == std::string::npos)
      return {};

   return path.substr(pos);
}

std::string path_cat(beast::string_view base, beast::string_view path)
{
   if (base.empty())
      return std::string(path);

   std::string result(base);
   char constexpr path_separator = '/';
   if(result.back() == path_separator)
      result.resize(result.size() - 1);
   result.append(path.data(), path.size());
   return result;
}

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

img_session::img_session(tcp::socket socket, std::string docroot)
: socket(std::move(socket))
, deadline {socket.get_executor(), std::chrono::seconds(60)}
, doc_root {std::move(docroot)}
{ }

void img_session::run()
{
   auto self = shared_from_this();
   auto f = [self](auto ec, auto n)
      { self->on_read_header(ec, n); };

   http::async_read_header(socket, buffer, header_parser, f);

   check_deadline();
}

void img_session::post_handler()
{
   auto const path = path_cat(doc_root, header_parser.get().target());
   std::cout << "Path: " << path << std::endl;

   auto is_valid = true; // TODO: Prove signature here.
   if (!is_valid) {
      // The filename does not contain a valid signature.
      resp.result(http::status::bad_request);
      resp.set(http::field::content_type, "text/plain");
      beast::ostream(resp.body()) << "Invalid signature.\r\n";
      resp.set(http::field::content_length, resp.body().size());
      write_response();
      return;
   }

   // The filename contains a valid signature, we can open the file
   // and read the body to save the image.

   body_parser = std::make_unique< req_body_parser_type
                                 >(std::move(header_parser));

   beast::error_code ec;
   body_parser->get().body().open( path.data()
                                 , beast::file_mode::write, ec);

   if (ec) {
      std::cout << ec.message() << std::endl;
      resp.result(http::status::bad_request);
      resp.set(http::field::content_type, "text/plain");
      beast::ostream(resp.body()) << "Error\r\n";
      resp.set(http::field::content_length, resp.body().size());
      write_response();
      return;
   }

   auto self = shared_from_this();
   auto f = [self](auto ec, auto n)
      { self->on_read_post_body(ec, n); };

   header_parser.body_limit(1000000);
   http::async_read(socket, buffer, *body_parser, f);
}

void img_session::get_handler()
{
   auto const path = path_cat(doc_root, header_parser.get().target());

   std::cout << "Http get: " << path << std::endl;

   if (std::empty(path)) {
      resp.result(http::status::not_found);
      resp.set(http::field::content_type, "text/plain");
      beast::ostream(resp.body()) << "File not found.\r\n";
      resp.set(http::field::content_length, resp.body().size());
      write_response();
      return;
   }

   beast::error_code ec;
   http::file_body::value_type body;
   body.open(path.data(), beast::file_mode::scan, ec);

   if (ec) {
      std::cout << ec.message() << std::endl;
      resp.result(http::status::not_found);
      resp.set(http::field::content_type, "text/plain");
      beast::ostream(resp.body()) << "File not found.\r\n";
      resp.set(http::field::content_length, resp.body().size());
      write_response();
      return;
   }

   auto const size = std::size(body);
   file_body_resp = std::make_unique<resp_body_type>(
      std::piecewise_construct,
      std::make_tuple(std::move(body)),
      std::make_tuple(http::status::ok,
         header_parser.get().version()));

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

void
img_session::on_read_post_body( boost::system::error_code ec
                              , std::size_t n)
{
   std::cout << "===> size: " << n << std::endl;

   if (ec) {
      // TODO: Use the correct code.
      resp.result(http::status::bad_request);
      resp.set(http::field::content_type, "text/plain");
      beast::ostream(resp.body()) << "File not found\r\n";
      std::cout << "Error: " << ec.message() << std::endl;
      std::cout << "docroot: " << doc_root << std::endl;
   } else {
      resp.result(http::status::ok);
      resp.set(http::field::server, "Beast");
   }

   resp.set(http::field::content_length, resp.body().size());
   write_response();
}

void img_session::on_read_header(boost::system::error_code ec, std::size_t n)
{
   resp.version(header_parser.get().version());
   resp.keep_alive(false);

   switch (header_parser.get().method()) {
   case http::verb::post: post_handler(); break;
   case http::verb::get: get_handler(); break;
   default:
   {
      resp.result(http::status::bad_request);
      resp.set(http::field::content_type, "text/plain");
      resp.set(http::field::content_length, resp.body().size());
      write_response();
   }
   break;
   }
}

void img_session::write_response()
{
   auto self = shared_from_this();
   auto f = [self](auto ec, auto)
   {
      self->socket.shutdown(tcp::socket::shutdown_send, ec);
      if (ec)
         std::cout << ec.message() << std::endl;

      self->deadline.cancel();
   };

   http::async_write(socket, resp, f);
}

void img_session::check_deadline()
{
   auto self = shared_from_this();
   auto f = [self](auto ec)
   {
      if (!ec)
          self->socket.close(ec);
   };

   deadline.async_wait(f);
}

}

