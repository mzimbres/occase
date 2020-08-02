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

namespace html
{

auto make_img(std::string const& url)
{
   std::string img;
   img += "<img src=\"";
   img += url;
   img += "\">";
   return img;
}

// The delete target has the form
//
//    /delete/from/post_id/password
//
auto
make_del_post_link( std::string const& db_host
                  , std::string const& from
                  , std::string const& post_id
                  , std::string const& pwd)
{
   auto const target = "/delete/"
                     + from    + "/"
                     + post_id + "/"
                     + pwd;
   std::string del;
   del += "<td>";
   del += "<a href=\"";
   del += db_host;
   del += target;
   del += "\">Delete</a>";
   del += "</td>";
   return del;
}

auto
make_next_link( std::string const& db_host
              , int post_id
              , std::string const& pwd)
{
   auto const target = "/posts/"
                     + std::to_string(post_id) + "/"
                     + pwd;
   std::string s;
   s += "<a href=\"";
   s += db_host;
   s += target;
   s += "\">Next</a>";
   return s;
}

auto
make_img_row( std::string const& db_host
            , post const& p
            , std::string const& pwd) noexcept
{
   try {
      std::string row;
      row += "<tr>";
      row += make_del_post_link( db_host
                               , p.from
                               , std::to_string(p.id)
                               , pwd);
      for (auto const& s : p.images) {
         row += "<td>";
         row += make_img(s);
         row += "</td>";
      }
      row += "</tr>";
      return row;
   } catch (...) {
   }

   return std::string{};
}

auto make_adm_page( std::string const& db_host
                  , std::vector<post> const& posts
                  , std::string const& pwd
                  , int post_id)
{
   // This is kind of weird, the database itself does not need to know
   // anything about the body of the post. But to monitor what users
   // post we have to parse the images here.

   auto acc = [&](auto init, auto const& p)
   {
      init += make_img_row(db_host, p, pwd);
      return init;
   };

   auto const rows =
      std::accumulate( std::begin(posts)
                     , std::end(posts)
                     , std::string{}
                     , acc);

   std::string table;
   table += "<table style=\"width:100%\">";
   table += rows;
   table += "</table>";

   std::string page;
   page += "<!DOCTYPE html>";
   page += "<html>";
   page += "<body>";
   page += "<h1>Occase administration pannel</h1>\n";
   page += "<p>Posts</p>";
   page += table;
   page += "\n";
   page += make_next_link(db_host, post_id + 25, pwd);
   page += "</body>";
   page += "</html>";

   return page;
}

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

   // Expects a target in the form
   //
   //    /posts/post_id/password
   //
   // or
   //
   //    /posts/post_id/password/
   //
   void get_posts_handler(std::string const& target) noexcept
   {
      try {
         std::vector<std::string> foo;
         boost::split(foo, target, boost::is_any_of("/"));

         if (std::size(foo) != 3) {
            log::write( log::level::debug
                      , "Error: get_posts_handler target has wrong size: {0}"
                      , std::size(foo));
            set_not_fount_header();
            return;
         }

         auto const& db = derived().db();

         if (foo.back() != db.get_cfg().adm_pwd) {
            log::write( log::level::debug
                      , "Error: get_posts_handler target has wrong password: {0}"
                      , foo.back());
            set_not_fount_header();
            return;
         }

         resp_.set(http::field::content_type, "text/html");

         auto const post_id = std::stoi(foo[1]);

         auto const posts =
            db.get_posts(post_id, [](auto const&){return true;});

         resp_.body() =
            html::make_adm_page( db.get_cfg().db_host
                               , posts
                               , db.get_cfg().adm_pwd
                               , post_id);

      } catch (std::exception const& e) {
         log::write( log::level::debug
                   , "get_posts_handler: {0}"
                   , e.what());
      }
   }

   // Expects a target in the from
   //
   // /delete/from/post_id/password
   //
   void get_delete_handler(std::string const& target) noexcept
   {
      try {
         std::vector<std::string> foo;
         boost::split(foo, target, boost::is_any_of("/"));

         if (std::size(foo) != 5) {
            log::write( log::level::debug
                      , "Error: get_delete_handler target has wrong size: {0}"
                      , std::size(foo));
            set_not_fount_header();
            return;
         }

         auto& db = derived().db();

         if (foo.back() != db.get_cfg().adm_pwd) {
            log::write( log::level::debug
                      , "Error: get_delete_handler target has wrong password: {0}"
                      , foo.back());
            set_not_fount_header();
            return;
         }

         db.delete_post(std::stoi(foo[2]), foo[1]);

         resp_.set(http::field::content_type, "text/html");
         resp_.body() = "Ok";

      } catch (std::exception const& e) {
         log::write( log::level::debug
                   , "get_delete_handler: {0}"
                   , e.what());
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

         resp_.set(http::field::content_type, "text/html");

	 if (only_count) {
	    auto const n = derived().db().count_posts(p);
	    resp_.body() = std::to_string(n) + "\r\n";
	 } else {
	    json j;
	    j["posts"] = derived().db().search_posts(p);
	    resp_.body() = j.dump() + "\r\n";
	 }

      } catch (std::exception const& e) {
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
         resp_.set(http::field::content_type, "text/html");
	 json j;
	 j["credit"] = derived().db().get_upload_credit();
	 resp_.body() = j.dump() + "\r\n";

      } catch (std::exception const& e) {
         log::write( log::level::err
                   , "post_upload_credit_handler (1): {0}"
                   , e.what());
         log::write( log::level::err
                   , "post_upload_credit_handler (2): {0}"
                   , req_.body());
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
         resp_.set(http::field::server, "occase-db");

         auto const t = prepare_target(req_.target(), '/');
         std::string const target {t.data(), std::size(t)};

         log::write(log::level::debug, "get_handler: {0}.", target);

         if (target == "stats") {
            stats_handler();
         } else if (target.compare(0, 5, "posts") == 0) {
            get_posts_handler(target);
         } else if (target.compare(0, 6, "delete") == 0) {
            get_delete_handler(target);
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
         resp_.set(http::field::server, "occase-db");

         auto const t = prepare_target(req_.target(), '/');
         std::string const target {t.data(), std::size(t)};

         log::write(log::level::debug, "post_handler: {0}.", target);

         if (target.compare(0, 5, "count") == 0) {
            post_search_handler(true);
	 } else if (target.compare(0, 6, "search") == 0) {
            post_search_handler();
	 } else if (target.compare(0, 13, "upload-credit") == 0) {
            post_upload_credit_handler();
	 } else {
            set_not_fount_header();
         }
      } catch (...) {
         set_not_fount_header();
      }

      do_write();
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

      resp_.set(http::field::content_length, resp_.body().size());
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

