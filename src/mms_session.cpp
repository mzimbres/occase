#include "mms_session.hpp"

#include <array>
#include <fstream>
#include <iostream>
#include <filesystem>

#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "post.hpp"
#include "utils.hpp"
#include "logger.hpp"
#include "crypto.hpp"

/* The http target used by apps to post images has the form generated
 * on db_worker::on_app_filenames plus perhaps a file extension the
 * may be added by the app.
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

namespace occase
{

using path_info = std::array<beast::string_view, 4>;

// Expects a target in the form
//
//    /x/x/xx/filename:digest.jpg
//
// where the extension is optional and returns an array in the form
//
// a[0] = x/x/xx
// a[1] = filename
// a[2] = digest
// a[3] = jpg
auto make_path_info(beast::string_view target)
{
   path_info pinfo;

   std::array<char, 4> delimiters {{'/', '/', pwd_gen::sep, '.'}};
   auto j = ssize(delimiters) - 1;
   auto k = ssize(target);
   for (auto i = k - 1; i >= 0; --i) {
      if (target[i] == delimiters[j]) {
         pinfo[j] = target.substr(i + 1, k - i - 1);
         k = i;
         --j;
      }

      if (j == 0) {
         pinfo[0] = target.substr(1, k - 1);
         break;
      }
   }

   return pinfo;
}

// This function tests if the target the digest has been indeed
// generated using the filename and the secret key shared only by
// occase db and the occase mms.
auto is_valid(path_info const& info, std::string const& mms_key)
{
   std::string path = "/";
   path.append(info[0].data(), std::size(info[0]));
   path += "/";
   path.append(info[1].data(), std::size(info[1]));

   auto const digest = make_hex_digest(path, mms_key);
   auto const digest_size = std::size(info[2]);
   if (digest_size != std::size(digest))
      return false;

   return digest.compare(0, digest_size, info[2].data(), digest_size) == 0;
}

beast::string_view mime_type(beast::string_view path)
{
    using beast::iequals;

    auto const ext = [&path]
    {
        auto const pos = path.rfind(".");
        if (pos == beast::string_view::npos)
           return beast::string_view{};
        return path.substr(pos);
    }();

    if (iequals(ext, ".htm"))  return "text/html";
    if (iequals(ext, ".html")) return "text/html";
    if (iequals(ext, ".php"))  return "text/html";
    if (iequals(ext, ".css"))  return "text/css";
    if (iequals(ext, ".txt"))  return "text/plain";
    if (iequals(ext, ".js"))   return "application/javascript";
    if (iequals(ext, ".json")) return "application/json";
    if (iequals(ext, ".xml"))  return "application/xml";
    if (iequals(ext, ".swf"))  return "application/x-shockwave-flash";
    if (iequals(ext, ".flv"))  return "video/x-flv";
    if (iequals(ext, ".png"))  return "image/png";
    if (iequals(ext, ".jpe"))  return "image/jpeg";
    if (iequals(ext, ".jpeg")) return "image/jpeg";
    if (iequals(ext, ".jpg"))  return "image/jpeg";
    if (iequals(ext, ".gif"))  return "image/gif";
    if (iequals(ext, ".bmp"))  return "image/bmp";
    if (iequals(ext, ".ico"))  return "image/vnd.microsoft.icon";
    if (iequals(ext, ".tiff")) return "image/tiff";
    if (iequals(ext, ".tif"))  return "image/tiff";
    if (iequals(ext, ".svg"))  return "image/svg+xml";
    if (iequals(ext, ".svgz")) return "image/svg+xml";

    return "application/text";
}

mms_session::mms_session( tcp::socket socket
                        , arg_type arg
                        , ssl::context& ctx)
: stream_(std::move(socket))
, worker_ {arg}
{ }

// TODO: Start the timeout here, not on the constructor.
void mms_session::run(std::chrono::seconds s)
{
   beast::get_lowest_layer(stream_).expires_after(s);

   auto self = shared_from_this();
   auto f = [self](auto ec, auto n)
      { self->on_read_header(ec, n); };

   header_parser.body_limit(worker_.get_cfg().body_limit);
   http::async_read_header(stream_, buffer, header_parser, f);
}

void mms_session::post_handler()
{
   auto const target = header_parser.get().target();

   // Before posting we check if the digest and the rest of the target
   // have been produced by the same key.
   std::string path;
   auto const pinfo = make_path_info(target);
   if (is_valid(pinfo, worker_.get_cfg().mms_key)) {
      path = worker_.get_cfg().doc_root;
      path.append(target.data(), std::size(target));
      log::write(log::level::debug , "Post dir: {0}", path);
      auto full_dir = worker_.get_cfg().doc_root + "/";
      full_dir.append(pinfo[0].data(), std::size(pinfo[0]));
      log::write(log::level::debug , "MMS dir: {0}", full_dir);
      create_dir(full_dir.data());
   }

   log::write(log::level::debug , "Post full dir: {0}", path);

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
                                 , beast::file_mode::write_new
                                 , ec);

   if (ec) {
      log::write(log::level::info , "post_handler: {0}", ec.message());
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

   http::async_read(stream_, buffer, *body_parser, f);
}

void mms_session::get_handler()
{
   auto const target = header_parser.get().target();
   log::write(log::level::debug, "Get target: {0}", target);

   // NOTE: Early, I was checking if the digest part of the path (see
   // db_worker::on_app_filenames) was indeed generated with the
   // filename and the secret key. But this has some drawbacks as it
   // does not allow us to change the key easily later. To simplify I
   // will always server the images and think about this later.

   auto path = worker_.get_cfg().doc_root;
   path.append(target.data(), std::size(target));

   log::write(log::level::debug, "Get target path: {0}", path);

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
      log::write(log::level::debug, "get_handler: {0}", ec.message());
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
      { self->stream_.socket().shutdown(tcp::socket::shutdown_send, ec); };

   http::async_write(stream_, *file_body_resp, f);
}

void
mms_session::on_read_post_body( boost::system::error_code ec
                              , std::size_t n)
{
   log::write(log::level::debug, "Body size: {0}.", n);

   if (ec) {
      // TODO: Use the correct code.
      resp.result(http::status::bad_request);
      resp.set(http::field::content_type, "text/plain");
      beast::ostream(resp.body()) << "File not found\r\n";
      log::write( log::level::info
                , "on_read_post_body: {0}."
                , ec.message());
   } else {
      resp.result(http::status::ok);
      resp.set(http::field::server, "occase-img");
   }

   resp.set(http::field::content_length, resp.body().size());
   write_response();
}

void mms_session::on_read_header(boost::system::error_code ec, std::size_t n)
{
   if (!log::ignore(log::level::debug)) { // Optimization.
      for (auto const& field : header_parser.get())
         log::write(log::level::debug, "Header: {0} = {1}", field.name(), field.value());
   }

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

void mms_session::write_response()
{
   auto self = shared_from_this();
   auto f = [self](auto ec, auto)
      { self->stream_.socket().shutdown(tcp::socket::shutdown_send, ec); };

   http::async_write(stream_, resp, f);
}

}

