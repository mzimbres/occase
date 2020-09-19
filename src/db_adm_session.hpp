#pragma once

#include <memory>
#include <chrono>
#include <algorithm>
#include <boost/algorithm/string.hpp>

#include "net.hpp"
#include "post.hpp"
#include "db_worker.hpp"

namespace occase
{

// Transfroms a target in the form
//
//    /a/b/.../c/ or a/b or /a/b/.../ or a/b/.../c/
//
// into
//
//   a/b/c
//
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

template <class AdmSession>
class db_worker;

struct worker_stats {
   int number_of_sessions = 0;
   int worker_post_queue_size = 0;
   int worker_reg_queue_size = 0;
   int worker_login_queue_size = 0;
   int db_post_queue_size = 0;
   int db_chat_queue_size = 0;
};

inline
std::ostream& operator<<(std::ostream& os, worker_stats const& stats)
{
   os << stats.number_of_sessions
      << ";"
      << stats.worker_post_queue_size
      << ";"
      << stats.worker_reg_queue_size
      << ";"
      << stats.worker_login_queue_size
      << ";"
      << stats.db_post_queue_size
      << ";"
      << stats.db_chat_queue_size;

   return os;
}

auto to_string(worker_stats const& stats)
{
   std::stringstream ss;
   ss << stats;
   return ss.str();
}

template <class Derived>
class db_adm_session {
protected:
   beast::flat_buffer buffer_{8192};

private:
   http::request<http::string_body> req_;
   http::response<http::string_body> resp_;

   Derived& derived() { return static_cast<Derived&>(*this); }

   void on_read(beast::error_code ec, std::size_t bytes_transferred)
   {
      boost::ignore_unused(bytes_transferred);

      if (ec == http::error::end_of_stream) {
         auto const n = derived().db().get_cfg().ssl_shutdown_timeout;
         return derived().do_eof(std::chrono::seconds {n});
      }

      if (ec) {
         log::write(log::level::debug, "db_adm_session: {0}", ec.message());
         return;
      }

      if (websocket::is_upgrade(req_)) {
         log::write(log::level::debug, "db_adm_session: Websocket upgrade");
         beast::get_lowest_layer(derived().stream()).expires_never();

         std::make_shared< typename Derived::db_session_type
                         >( std::move(derived().release_stream())
                          , derived().db()
                          )->run(std::move(req_));
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
	    p = j.get<post>();
	 }

         resp_.set(http::field::content_type, "application/json");

	 if (only_count) {
	    auto const n = derived().db().count_posts(p);
	    resp_.body() = std::to_string(n) + "\r\n";
	 } else {
	    json j;
	    j["posts"] = derived().db().search_posts(p);
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
	 j["credit"] = derived().db().get_upload_credit();
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

	 derived().db().delete_post(from, key, post_id);

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
         auto const body = derived().db().on_publish(json::parse(req_.body()));
         resp_.set(http::field::content_type, "application/json");
	 resp_.body() = body + "\r\n";
      } catch (std::exception const& e) {
         set_not_fount_header();
         log::write( log::level::err
                   , "post_publish_handler: {0}"
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
         resp_.set(http::field::server, derived().db().get_cfg().server_name);

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
	 if (std::empty(req_.body())) {
            set_not_fount_header();
            do_write();
	    return;
         }

         resp_.result(http::status::ok);
         resp_.set(http::field::server, derived().db().get_cfg().server_name);

         auto const t = req_.target();

         std::string const target {t.data(), std::size(t)};
         log::write(log::level::debug, "post_handler.");

         char const count[]  = "/posts/count";
         char const search[] = "/posts/search";
         char const upload[] = "/posts/upload-credit";
         char const del[]    = "/posts/delete";
         char const pub[]    = "/posts/publish";
	 char const visua[] =  "/posts/visualizations";
	 char const click[] =  "/posts/click";

         if (t.compare(0, sizeof count, count) == 0) {
            post_search_handler(true);
	 } else if (t.compare(0, sizeof visua, visua) == 0) {
            post_visualization_handler();
	 } else if (t.compare(0, sizeof click, click) == 0) {
            post_click_handler();
	 } else if (t.compare(0, sizeof search, search) == 0) {
            post_search_handler();
	 } else if (t.compare(0, sizeof upload, upload) == 0) {
            post_upload_credit_handler();
	 } else if (t.compare(0, sizeof del, del) == 0) {
            post_delete_handler();
	 } else if (t.compare(0, sizeof pub, pub) == 0) {
            post_publish_handler();
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
	 derived().db().on_visualizations(req_.body());
         resp_.set(http::field::content_type, "text/plain");
	 resp_.body() = "Ok\r\n";
      } catch (std::exception const& e) {
         set_not_fount_header();
         log::write( log::level::err
                   , "post_visualization_handler: {0}"
                   , e.what());
      }
   }

   void post_click_handler() noexcept
   {
      try {
	 derived().db().on_click(req_.body());
         resp_.set(http::field::content_type, "text/plain");
	 resp_.body() = "Ok\r\n";
      } catch (std::exception const& e) {
         set_not_fount_header();
         log::write( log::level::err
                   , "post_click_handler: {0}"
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
      resp_.body() = to_string(derived().db().get_stats());
   }

   void do_write()
   {
      auto self = derived().shared_from_this();

      resp_.set(http::field::content_length,
                beast::to_static_string(std::size(resp_.body())));
      resp_.set(http::field::access_control_allow_origin,
	        derived().db().get_cfg().http_allow_origin);

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
                   , "Error on db_adm_session: {0}"
                   , ec.message());
      }
      auto const n = derived().db().get_cfg().ssl_shutdown_timeout;
      return derived().do_eof(std::chrono::seconds {n});
   }

public:
   void start()
   {
     do_read();
   }
};

}

