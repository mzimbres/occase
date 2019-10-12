#include "img_session.hpp"

#include <fstream>
#include <iostream>
#include <filesystem>

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "json_utils.hpp"

/* The http target used by apps to post images has the following form.
 *
 *    a/b.c
 *
 * The different parts have the following meaning.
 *
 * a: The hex digest produces by the ws server that uses as input b
 *    and a key that is known only by the ws and the image server. If
 *    a and b do not belong to each other the image has not been
 *    authorized by the server to be posted.
 *
 * b: The is the file name produced by the ws server. It is randonly
 *    generated.
 * 
 * c: The file extension. 
 *
 */

namespace {

void create_dir(const char *dir)
{
   char tmp[256];
   char *p = NULL;
   size_t len;

   snprintf(tmp, sizeof(tmp), "%s", dir);
   len = strlen(tmp);

   if (tmp[len - 1] == '/')
      tmp[len - 1] = 0;

   for (p = tmp + 1; *p; p++)
      if (*p == '/') {
         *p = 0;
         mkdir(tmp, 0777);
         *p = '/';
      }

   mkdir(tmp, 0777);
}

}

namespace rt
{

/* Can be used to get the filename or the file extension.
*
*    s = "/" for file name.
*    s = "." for file extension.
*
* For example foo.bar will result in "foo" and "bar".
*/

std::pair<beast::string_view, beast::string_view>
split(beast::string_view path, char const* s)
{
   auto const pos = path.rfind(s);
   if (pos == std::string::npos)
      return {};

   return {path.substr(0, pos), path.substr(pos + 1)};
}

/* This function receives as input the filename as specified above in
 * the form a/b.c, that means B and returns the path where it should
 * be stored, which will be a string with the form.
 *
 *    d/e/f
 */
std::string make_img_path(beast::string_view filename)
{
   std::string path;
   path.append(filename.data(), 0, sz::a);
   path.push_back('/');
   path.append(filename.data(), sz::a, sz::b);
   path.push_back('/');
   path.append(filename.data(), sz::b, sz::c);

   return path;
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

img_session::img_session( tcp::socket socket
                        , arg_type arg
                        , ssl::context& ctx)
: socket(std::move(socket))
, deadline {socket.get_executor(), std::chrono::seconds(60)}
, cfg {arg}
{ }

void img_session::run()
{
   std::cout << "Accept" << std::endl;

   auto self = shared_from_this();
   auto f = [self](auto ec, auto n)
      { self->on_read_header(ec, n); };

   header_parser.body_limit(10000000);
   http::async_read_header(socket, buffer, header_parser, f);

   check_deadline();
}

struct splited_target {
   beast::string_view digest;
   beast::string_view filename;
   beast::string_view extension;
};

splited_target make_splited_target(beast::string_view const target)
{
   // Splits the target in the form [A, B.C].
   auto const pair1 = split(target, "/");

   // Splits the filename from the extension, we end up with [B, C]
   auto const pair2 = split(pair1.second, ".");

   return {pair1.first, pair2.first, pair2.second};
}

auto is_valid(splited_target const& st)
{
   if (std::size(st.filename) < sz::img_filename_min_size)
      return false;

   // TODO: Prove signature and sizes here.
   return !std::empty(st.digest) && !std::empty(st.extension);
}

void img_session::post_handler()
{
   std::string path;

   auto const st = make_splited_target(header_parser.get().target());
   if (is_valid(st)) {
      path = cfg.doc_root + "/" + make_img_path(st.filename);

      std::cout << "Dir: " << path << std::endl;

      create_dir(path.data());

      path += "/";
      path.append(st.filename.data(), std::size(st.filename));
      path += ".";
      path.append(st.extension.data(), std::size(st.extension));
   }

   std::cout << "Path: " << path << std::endl;

   if (std::empty(path)) {
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

   http::async_read(socket, buffer, *body_parser, f);
}

void img_session::get_handler()
{
   std::cout << "Get target: " << header_parser.get().target() << std::endl;

   std::string path;

   auto const st = make_splited_target(header_parser.get().target());
   if (is_valid(st)) {
      path = cfg.doc_root + "/" + make_img_path(st.filename) + "/";
      path.append(st.filename.data(), std::size(st.filename));
      path += ".";
      path.append(st.extension.data(), std::size(st.extension));
   }

   std::cout << "Path: " << path << std::endl;

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
      std::cout << "docroot: " << cfg.doc_root << std::endl;
   } else {
      resp.result(http::status::ok);
      resp.set(http::field::server, "Beast");
   }

   resp.set(http::field::content_length, resp.body().size());
   write_response();
}

void img_session::on_read_header(boost::system::error_code ec, std::size_t n)
{
   for (auto const& field : header_parser.get())
      std::cout << field.name() << " = " << field.value() << "\n";

   std::cout << "on_read_header" << std::endl;
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

