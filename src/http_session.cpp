#include "http_session.hpp"

#include <chrono>
#include <memory>

namespace rt
{

http_session::http_session(tcp::socket socket , worker const& w)
: socket_ {std::move(socket)}
, deadline_ { socket_.get_executor()
            , std::chrono::seconds(60)}
, worker_ {w}
{ }

void http_session::read_request()
{
   auto self = shared_from_this();
   auto handler = [self](beast::error_code ec, std::size_t n)
   {
      boost::ignore_unused(n);

      if (!ec)
          self->process_request();
   };

   http::async_read(socket_, buffer_, request_, handler);
}

void http_session::process_request()
{
   response_.version(request_.version());
   response_.keep_alive(false);

   switch(request_.method()) {
   case http::verb::get:
   {
      response_.result(http::status::ok);
      response_.set(http::field::server, "Beast");
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

void http_session::create_response()
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

void http_session::write_response()
{
   auto self = shared_from_this();

   response_.set(http::field::content_length, response_.body().size());

   auto handler = [self](beast::error_code ec, std::size_t)
   {
      self->socket_.shutdown(tcp::socket::shutdown_send, ec);
      self->deadline_.cancel();
   };

   http::async_write(socket_, response_, handler);
}

void http_session::check_deadline()
{
    auto self = shared_from_this();

    auto handler = [self](boost::beast::error_code ec)
    {
        if (!ec)
           self->socket_.close(ec);
    };

    deadline_.async_wait(handler);
}

void http_session::start()
{
    read_request();
    check_deadline();
}

}

