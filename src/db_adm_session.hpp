#pragma once

#include <memory>
#include <chrono>
#include <memory>

#include "net.hpp"
#include "post.hpp"
#include "db_worker.hpp"

namespace rt
{

auto make_adm_page(std::vector<post> const& posts)
{
   json j;
   j["items"] = posts;

   std::string page;
   page += "<!DOCTYPE html>\n";
   page += "<html>\n";
   page += "<body>\n";
   page += "\n";
   page += "<h1>Occase administration pannel</h1>\n";
   page += "\n";
   page += "<p>Posts.</p>\n";
   page += "\n";
   page += j.dump();
   page += "\n";
   page += "<a href=\"http://127.0.0.1:9091/delete\">This is a link</a>";
   page += "</body>\n";
   page += "</html>\n";

   return page;
}

auto post_del_ok()
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
   using arg_type = worker_type const&;

private:
   tcp::socket socket_;
   beast::flat_buffer buffer_{8192};
   http::request<http::dynamic_body> request_;
   http::response<http::dynamic_body> response_;
   net::steady_timer deadline_;
   db_worker<Session> const& worker_;
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

   void get_handler()
   {
      response_.result(http::status::ok);
      response_.set(http::field::server, "occase-db");

      if (request_.target() == "/stats") {
         stats = worker_.get_stats();
         response_.set(http::field::content_type, "text/csv");
         boost::beast::ostream(response_.body())
         << stats
         << "\n";
      } else if (request_.target() == "/posts") {
         log(loglevel::debug, "Posts have requested.");
         auto const posts = worker_.get_posts(0);
         response_.set(http::field::content_type, "text/html");
         boost::beast::ostream(response_.body())
         << make_adm_page(posts);
      } else if (request_.target() == "/delete") {
         log(loglevel::info, "Post deletion.");
         // TODO: Check credentials to allow post deletion.
         response_.set(http::field::content_type, "text/html");
         boost::beast::ostream(response_.body())
         << post_del_ok();
      } else {
         response_.result(http::status::not_found);
         response_.set(http::field::content_type, "text/plain");
         beast::ostream(response_.body()) << "File not found\r\n";
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

