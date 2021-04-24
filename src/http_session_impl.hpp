#pragma once

#include <memory>
#include <chrono>
#include <algorithm>
#include <boost/algorithm/string.hpp>

#include "net.hpp"
#include "post.hpp"
#include "worker.hpp"
#include "ws_session.hpp"

namespace occase
{

inline
void make_ssl_session(ssl_stream stream, worker& w, request_type req)
{
   auto* p = new ws_session<ssl_stream>(std::move(stream), w);
   std::shared_ptr<ws_session_base> sp(p);
   sp->run(std::move(req));
}

inline
void make_ssl_session(tcp_stream, worker&, request_type)
{
   // Noop
}

inline
void make_plain_session(tcp_stream stream, worker& w, request_type req)
{
   auto* p = new ws_session<tcp_stream>(std::move(stream), w);
   std::shared_ptr<ws_session_base> sp(p);
   sp->run(std::move(req));
}

inline
void make_plain_session(ssl_stream, worker&, request_type)
{
   // Noop
}

// Transfroms a target in the form
//
//    /a/b/.../c/ or a/b or /a/b/.../ or a/b/.../c/
//
// into
//
//   a/b/c
//
inline
beast::string_view prepare_target(beast::string_view t, char s)
{
   if (std::empty(t))
      return {};

   auto const b1 = t.front() == s;
   auto const b2 = t.back() == s;

   if (b1 && b2)
      return t.substr(1, std::size(t) - 1);

   if (b1)
      return t.substr(1, std::size(t));

   if (b2)
      return t.substr(0, std::size(t) - 1);

   return t;
}

template <class Derived>
class http_session_impl {
protected:
   beast::flat_buffer buffer_{8192};
   worker& w_;

private:
   request_type req_;
   http::response<http::string_body> resp_;

   Derived& derived() { return static_cast<Derived&>(*this); }

   void on_read(beast::error_code ec, std::size_t bytes_transferred)
   {
      boost::ignore_unused(bytes_transferred);

      if (ec == http::error::end_of_stream) {
         auto const n = w_.get_cfg().ssl_shutdown_timeout;
         return derived().do_eof(std::chrono::seconds {n});
      }

      if (ec) {
         log::write(log::level::debug, "http_session_impl: {0}", ec.message());
         return;
      }

      if (websocket::is_upgrade(req_)) {
         log::write(log::level::debug, "http_session_impl: Websocket upgrade");
         beast::get_lowest_layer(derived().stream()).expires_never();

	 if (derived().is_ssl()) {
	    make_ssl_session(derived().release_stream(), w_, std::move(req_));
	 } else {
	    make_plain_session(derived().release_stream(), w_, std::move(req_));
	 }

         return;
      }

      process_request();
   }

   void do_read()
   {
      auto self = derived().shared_from_this();

      auto handler = [self](auto ec, auto n)
         { self->on_read(ec, n); };

      http::async_read(derived().stream(), buffer_, req_, handler);
   }

   void process_request()
   {
      resp_.version(req_.version());
      resp_.keep_alive(false);

      switch (req_.method()) {
         case http::verb::get: get_handler();  break;
         case http::verb::post: post_handler();  break;
         default: default_handler(); break;
      }
   }

   void post_search_handler(bool only_count = false) noexcept
   {
      try {
	 post p;
	 if (!std::empty(req_.body())) {
	    auto const j = json::parse(req_.body());
	    p = j.at("post").get<post>();
	 }

         resp_.set(http::field::content_type, "application/json");

	 if (only_count) {
	    auto const n = w_.count_posts(p);
	    resp_.body() = std::to_string(n) + "\r\n";
	 } else {
	    json j;
	    j["posts"] = w_.search_posts(p);
	    resp_.body() = j.dump() + "\r\n";
	 }

      } catch (std::exception const& e) {
         set_not_fount_header();
         log::write( log::level::err
                   , "post_search_handler (1): {0}"
                   , e.what());
         log::write( log::level::err
                   , "post_search_handler (2): {0}"
                   , req_.body());
      }
   }

   void post_upload_credit_handler() noexcept
   {
      try {
         resp_.set(http::field::content_type, "application/json");
	 json j;
	 j["credit"] = w_.get_upload_credit();
	 resp_.body() = j.dump() + "\r\n";

      } catch (std::exception const& e) {
         set_not_fount_header();
         log::write( log::level::err
                   , "post_upload_credit_handler (1): {0}"
                   , e.what());
         log::write( log::level::err
                   , "post_upload_credit_handler (2): {0}"
                   , req_.body());
      }
   }

   void post_delete_handler() noexcept
   {
      try {
	 auto const j = json::parse(req_.body());
	 auto const from = j.at("user").get<std::string>();
	 auto const key = j.at("key").get<std::string>();
	 auto const post_id = j.at("post_id").get<std::string>();

	 w_.delete_post(from, key, post_id);

         resp_.set(http::field::content_type, "text/plain");
	 resp_.body() = "Ok\r\n";

      } catch (std::exception const& e) {
         set_not_fount_header();
         log::write( log::level::err
                   , "post_delete_handler: {0}"
                   , e.what());
      }
   }

   void post_publish_handler() noexcept
   {
      try {
         auto const body = w_.on_publish_impl(json::parse(req_.body()));
         resp_.set(http::field::content_type, "application/json");
	 resp_.body() = body + "\r\n";
      } catch (std::exception const& e) {
         set_not_fount_header();
         log::write( log::level::err
                   , "post_publish_handler: {0}"
                   , e.what());
      }
   }

   void get_user_id_handler() noexcept
   {
      try {
         auto const body = w_.on_get_user_id();
         resp_.set(http::field::content_type, "application/json");
	 resp_.body() = body + "\r\n";
      } catch (std::exception const& e) {
         set_not_fount_header();
         log::write( log::level::err
                   , "get_user_id_handler: {0}"
                   , e.what());
      }
   }

   void set_not_fount_header()
   {
      resp_.result(http::status::not_found);
      resp_.set(http::field::content_type, "text/plain");
      resp_.body() = "File not found\r\n";
   }

   void get_handler()
   {
      try {
         resp_.result(http::status::ok);
         resp_.set(http::field::server, w_.get_cfg().server_name);

         auto const t = prepare_target(req_.target(), '/');
         std::string const target {t.data(), std::size(t)};

         log::write(log::level::debug, "get_handler: {0}.", target);

         if (target == "stats") {
            stats_handler();
         } else {
            set_not_fount_header();
         }
      } catch (...) {
         set_not_fount_header();
      }

      do_write();
   }

   void post_handler()
   {
      try {
         resp_.result(http::status::ok);
         resp_.set(http::field::server, w_.get_cfg().server_name);

         auto const t = req_.target();

         std::string const target {t.data(), std::size(t)};

         char const count[]  = "/posts/count";
         char const search[] = "/posts/search";
         char const upload[] = "/posts/upload-credit";
         char const del[]    = "/posts/delete";
         char const pub[]    = "/posts/publish";
	 char const visua[] =  "/posts/visualization";
	 char const get_user_id[] = "/get-user-id";

         if (t.compare(0, sizeof count, count) == 0) {
            post_search_handler(true);
	 } else if (t.compare(0, sizeof visua, visua) == 0) {
            post_visualization_handler();
	 } else if (t.compare(0, sizeof search, search) == 0) {
            post_search_handler();
	 } else if (t.compare(0, sizeof upload, upload) == 0) {
            post_upload_credit_handler();
	 } else if (t.compare(0, sizeof del, del) == 0) {
            post_delete_handler();
	 } else if (t.compare(0, sizeof pub, pub) == 0) {
            post_publish_handler();
	 } else if (t.compare(0, sizeof get_user_id, get_user_id) == 0) {
            get_user_id_handler();
	 } else {
            set_not_fount_header();
         }
      } catch (...) {
         set_not_fount_header();
      }

      do_write();
   }

   void post_visualization_handler() noexcept
   {
      try {
	 w_.on_visualization(req_.body());
         resp_.set(http::field::content_type, "text/plain");
	 resp_.body() = "Ok\r\n";
      } catch (std::exception const& e) {
         set_not_fount_header();
         log::write( log::level::err
                   , "post_visualization_handler: {0}"
                   , e.what());
      }
   }

   void default_handler()
   {
      resp_.result(http::status::bad_request);
      resp_.set(http::field::content_type, "text/plain");
      resp_.body() = "Invalid request-method: '" + req_.method_string().to_string() +  "'";
      do_write();
   }

   void stats_handler()
   {
      resp_.set(http::field::content_type, "text/csv");
      resp_.body() = to_string(w_.get_stats());
   }

   void do_write()
   {
      auto self = derived().shared_from_this();

      resp_.set(http::field::content_length,
                beast::to_static_string(std::size(resp_.body())));
      resp_.set(http::field::access_control_allow_origin,
	        w_.get_cfg().http_allow_origin);

      auto handler = [self](auto ec, std::size_t n)
         { self->on_write(ec, n); };

      http::async_write(derived().stream(), resp_, handler);
   }

   void
   on_write(beast::error_code ec, std::size_t bytes_transferred)
   {
      boost::ignore_unused(bytes_transferred);

      if (ec) {
         log::write( log::level::debug
                   , "Error on http_session_impl: {0}"
                   , ec.message());
      }
      auto const n = w_.get_cfg().ssl_shutdown_timeout;
      return derived().do_eof(std::chrono::seconds {n});
   }

public:
   http_session_impl(worker& w, beast::flat_buffer buffer)
   : buffer_(std::move(buffer))
   , w_ {w}
   { }

   void start()
   {
     do_read();
   }
};

} // occase
