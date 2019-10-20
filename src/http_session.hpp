#pragma once

#include <memory>
#include <chrono>
#include <memory>

#include "worker.hpp"
#include "net.hpp"

namespace rt
{

template <class Session>
class worker;

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
class http_session :
   public std::enable_shared_from_this<http_session<Session>> {
public:
   using worker_type = worker<Session>;
   using arg_type = worker_type const&;

private:
   tcp::socket socket_;
   beast::flat_buffer buffer_{8192};
   http::request<http::dynamic_body> request_;
   http::response<http::dynamic_body> response_;
   net::steady_timer deadline_;
   worker<Session> const& worker_;
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

      switch(request_.method()) {
      case http::verb::get:
      {
         response_.result(http::status::ok);
         response_.set(http::field::server, "occase-db");
         stats = worker_.get_stats();
         create_response();
         write_response();
      }
      break;

      default:
      {
         // We return responses indicating an error if
         // we do not recognize the request method.
         response_.result(http::status::bad_request);
         response_.set(http::field::content_type, "text/plain");
         beast::ostream(response_.body())
             << "Invalid request-method '"
             << request_.method_string().to_string()
             << "'";
         write_response();
      }
      break;
      }
   }

   void create_response()
   {
      if (request_.target() == "/stats") {
         response_.set(http::field::content_type, "text/csv");
         boost::beast::ostream(response_.body())
             << stats
             << "\n";

      } else {
         response_.result(http::status::not_found);
         response_.set(http::field::content_type, "text/plain");
         beast::ostream(response_.body()) << "File not found\r\n";
      }
   }

   void write_response()
   {
      auto self = this->shared_from_this();

      response_.set(http::field::content_length, response_.body().size());

      auto handler = [self](beast::error_code ec, std::size_t)
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
   http_session(tcp::socket socket, arg_type w, ssl::context& ctx)
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

