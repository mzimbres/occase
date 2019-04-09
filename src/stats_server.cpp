#include "stats_server.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <string>

#include "config.hpp"

class http_session
   : public std::enable_shared_from_this<http_session>
{
public:
    http_session(tcp::socket socket)
    : socket_ {std::move(socket)}
    , deadline_ { socket_.get_executor().context()
                , std::chrono::seconds(60)}
    { }

    void start()
    {
        read_request();
        check_deadline();
    }

private:
    tcp::socket socket_;
    boost::beast::flat_buffer buffer_{8192};
    http::request<http::dynamic_body> request_;
    http::response<http::dynamic_body> response_;
    boost::asio::basic_waitable_timer<std::chrono::steady_clock> deadline_;

    void read_request()
    {
        auto handler =
           [p = shared_from_this()](beast::error_code ec, std::size_t n)
        {
           boost::ignore_unused(n);

           if(!ec)
               p->process_request();
        };

        http::async_read(socket_, buffer_, request_, handler);
    }

    void process_request()
    {
        response_.version(request_.version());
        response_.keep_alive(false);

        switch(request_.method())
        {
        case http::verb::get:
            response_.result(http::status::ok);
            response_.set(http::field::server, "Beast");
            create_response();
            break;

        default:
            // We return responses indicating an error if
            // we do not recognize the request method.
            response_.result(http::status::bad_request);
            response_.set(http::field::content_type, "text/plain");
            boost::beast::ostream(response_.body())
                << "Invalid request-method '"
                << request_.method_string().to_string()
                << "'";
            break;
        }

        write_response();
    }

    void create_response()
    {
        if (request_.target() == "/count") {
            response_.set(http::field::content_type, "text/html");
            boost::beast::ostream(response_.body())
                << "<html>\n"
                <<  "<head><title>Request count</title></head>\n"
                <<  "<body>\n"
                <<  "<h1>Request count</h1>\n"
                <<  "<p>There have been "
                <<  10
                <<  " requests so far.</p>\n"
                <<  "</body>\n"
                <<  "</html>\n";
        } else if(request_.target() == "/time") {
            response_.set(http::field::content_type, "text/html");
            boost::beast::ostream(response_.body())
                <<  "<html>\n"
                <<  "<head><title>Current time</title></head>\n"
                <<  "<body>\n"
                <<  "<h1>Current time</h1>\n"
                <<  "<p>The current time is "
                <<  " seconds since the epoch.</p>\n"
                <<  "</body>\n"
                <<  "</html>\n";
        } else {
            response_.result(http::status::not_found);
            response_.set(http::field::content_type, "text/plain");
            boost::beast::ostream(response_.body()) << "File not found\r\n";
        }
    }

    void write_response()
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

    // Check whether we have spent enough time on this connection.
    void check_deadline()
    {
        auto self = shared_from_this();

        auto handler = [self](boost::beast::error_code ec)
        {
            if (!ec)
               self->socket_.close(ec);
        };

        deadline_.async_wait(handler);
    }
};

void http_server(tcp::acceptor& acceptor, tcp::socket& socket)
{
   auto handler = [&](boost::beast::error_code ec)
   {
      if (ec) {
         if (ec == net::error::operation_aborted) {
            //log(loglevel::info, "Stopping accepting connections");
            return;
         }

         //log( loglevel::debug
         //   , "listener::on_accept: {0}"
         //   , ec.message());

         //return;
      } else {
         std::make_shared<http_session>(std::move(socket))->start();
      }

      http_server(acceptor, socket);
   };

   acceptor.async_accept(socket, handler);
}

stats_server::stats_server( char const* addr, unsigned short port
                          , net::io_context& ioc)
: acceptor_ {ioc, {net::ip::make_address(addr), port}}
, socket_ {ioc}
{
   run();
}

void stats_server::run()
{
   http_server(acceptor_, socket_);
}

void stats_server::shutdown()
{
   acceptor_.cancel();
}
