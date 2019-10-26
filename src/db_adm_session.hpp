#pragma once

#include <memory>
#include <chrono>
#include <algorithm>

#include "net.hpp"
#include "post.hpp"
#include "db_worker.hpp"

namespace rt
{

struct target4 {
   std::string a;
   std::string b;
   std::string c;
   std::string d;
};

target4 split4(beast::string_view target)
{
   // /a/b/c/d
   auto const foo = split(target, '/');

   auto const d =
      std::string( foo.second.data()
                 , std::size(foo.second));

   // /a/b/c
   auto const bar = split(foo.first, '/');

   auto const c =
      std::string( bar.second.data()
                 , std::size(bar.second));

   // /a/b
   auto const foobar = split(bar.first, '/');

   auto const b =
      std::string( foobar.second.data()
                 , std::size(bar.second));

   auto const a =
      std::string( foobar.first.data()
                 , std::size(bar.first));

   return {a, b, c, d};
}

namespace html
{

auto make_img(std::string const& mms_host, std::string const& name)
{
   std::string img;
   img += "<img src=\"";
   img += mms_host;
   img += name;
   img += "\">";
   return img;
}

auto
make_del_post_link( std::string const& adm_host
                  , std::string const& from
                  , std::string const& to
                  , std::string const& post_id)
{
   // The delete target has the form /delete/from/to/post_id
   auto const target = "delete/" + from + "/" + to + "/" + post_id;
   std::string del;
   del += "<td>";
   del += "<a href=\"";
   del += adm_host;
   del += target;
   del += "\">Delete</a>";
   del += "</td>";
   return del;
}

auto
make_img_row( std::string const& mms_host
            , std::string const& adm_host
            , post const& p) noexcept
{
   try {
      auto const j = json::parse(p.body);
      auto const images = j.at("images").get<std::vector<std::string>>();
      std::string row;
      row += "<tr>";
      row += make_del_post_link( adm_host
                               , p.from
                               , std::to_string(p.to)
                               , std::to_string(p.id));
      for (auto const& s : images) {
         row += "<td>";
         row += make_img(mms_host, s);
         row += "</td>";
      }
      row += "</tr>";
      return row;
   } catch (...) {
   }

   return std::string{};
}

auto make_adm_page( std::string const& mms_host
                  , std::string const& adm_host
                  , std::vector<post> const& posts)
{
   // This is kind of weird, the database itself does not need to know
   // anything about the body of the post. But to monitor what users
   // post we have to parse the images here.

   auto acc = [&](auto init, auto const& p)
   {
      init += make_img_row(mms_host, adm_host, p);
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
   page += "</body>";
   page += "</html>";

   return page;
}

auto make_post_del_ok()
{
   std::string page;
   page += "<!DOCTYPE html>\n";
   page += "<html>\n";
   page += "<body>\n";
   page += "\n";
   page += "<p>Post deletion: Success.</p>\n";
   page += "\n";
   page += "</body>\n";
   page += "</html>\n";

   return page;
}

}

template <class Session>
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

template <class Session>
class db_adm_session :
   public std::enable_shared_from_this<db_adm_session<Session>> {
public:
   using worker_type = db_worker<Session>;
   using arg_type = worker_type&;

private:
   tcp::socket socket_;
   beast::flat_buffer buffer_{8192};
   http::request<http::dynamic_body> request_;
   http::response<http::dynamic_body> response_;
   net::steady_timer deadline_;
   arg_type worker_;
   worker_stats stats {};

   void read_request()
   {
      auto self = this->shared_from_this();
      auto handler = [self](beast::error_code ec, std::size_t n)
      {
         boost::ignore_unused(n);

         if (!ec)
             self->process_request();
      };

      http::async_read(socket_, buffer_, request_, handler);
   }

   void process_request()
   {
      response_.version(request_.version());
      response_.keep_alive(false);

      switch (request_.method()) {
         case http::verb::get:  get_handler();  break;
         default: default_handler(); break;
      }
   }

   // Expects a target in the form
   //
   //    /posts/post_id
   //
   // or
   //
   //    /posts/post_id/
   //
   void get_posts_handler(beast::string_view target)
   {
      auto const foo = split(target, '/');
      auto const str =
         std::string( foo.second.data()
                    , std::size(foo.second));

      auto const posts = worker_.get_posts(std::stoi(str));
      response_.set(http::field::content_type, "text/html");
      boost::beast::ostream(response_.body())
      << html::make_adm_page( worker_.get_cfg().mms_host
                            , worker_.get_cfg().adm_host
                            , posts);
   }

   void get_delete_handler(beast::string_view target)
   {
      auto const t4 = split4(target);
      auto const post_id = std::stoi(t4.d);
      code_type const to = std::stoll(t4.c);
      worker_.delete_post(post_id, t4.b, to);

      response_.set(http::field::content_type, "text/html");
      boost::beast::ostream(response_.body())
      << html::make_post_del_ok();
   }

   void get_default_handler()
   {
      response_.result(http::status::not_found);
      response_.set(http::field::content_type, "text/plain");
      beast::ostream(response_.body()) << "File not found\r\n";
   }

   void get_handler()
   {
      try {
         response_.result(http::status::ok);
         response_.set(http::field::server, "occase-db");

         auto const target = request_.target();
         std::cout << target << std::endl;

         if (target == "/stats") {
            stats = worker_.get_stats();
            response_.set(http::field::content_type, "text/csv");
            boost::beast::ostream(response_.body())
            << stats
            << "\n";
         } else if (target.compare(0, 6, "/posts") == 0) {
            log(loglevel::debug, "Http post request received.");
            get_posts_handler(target);
         } else if (target.compare(0, 7, "/delete") == 0) {
            log(loglevel::info, "Post deletion.");
            get_delete_handler(target);
         } else {
            get_default_handler();
         }
      } catch (...) {
         get_default_handler();
      }

      write_response();
   }

   void default_handler()
   {
      response_.result(http::status::bad_request);
      response_.set(http::field::content_type, "text/plain");
      beast::ostream(response_.body())
          << "Invalid request-method '"
          << request_.method_string().to_string()
          << "'";
      write_response();
   }

   void write_response()
   {
      auto self = this->shared_from_this();

      response_.set(http::field::content_length, response_.body().size());

      auto handler = [self](auto ec, std::size_t)
      {
         self->socket_.shutdown(tcp::socket::shutdown_send, ec);
         self->deadline_.cancel();
      };

      http::async_write(socket_, response_, handler);
   }

   void check_deadline()
   {
      auto self = this->shared_from_this();

      auto handler = [self](boost::beast::error_code ec)
      {
          if (!ec)
             self->socket_.close(ec);
      };

      deadline_.async_wait(handler);
   }


public:
   db_adm_session(tcp::socket socket, arg_type w, ssl::context& ctx)
   : socket_ {std::move(socket)}
   , deadline_ { socket_.get_executor(), std::chrono::seconds(60)}
   , worker_ {w}
   { }

   void run()
   {
       read_request();
       check_deadline();
   }
};

}

