#include "stats_server.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "config.hpp"
#include "logger.hpp"
#include "worker.hpp"

namespace rt {

class http_session
   : public std::enable_shared_from_this<http_session>
{
private:
    tcp::socket socket_;
    boost::beast::flat_buffer buffer_{8192};
    http::request<http::dynamic_body> request_;
    http::response<http::dynamic_body> response_;
    net::steady_timer deadline_;
    worker const& worker_;

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
        if (request_.target() == "/stats") {
            response_.set(http::field::content_type, "text/csv");
            boost::beast::ostream(response_.body())
                << worker_.get_ws_stats().number_of_sessions
                << ";"
                << worker_.get_post_queue_size()
                << ";"
                << worker_.get_reg_queue_size()
                << ";"
                << worker_.get_login_queue_size()
                << ";"
                << worker_.get_db().get_post_queue_size()
                << ";"
                << worker_.get_db().get_chat_queue_size()
                << "\n";
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

public:
    http_session(tcp::socket socket, worker const& w)
    : socket_ {std::move(socket)}
    , deadline_ { socket_.get_executor().context()
                , std::chrono::seconds(60)}
    , worker_ {w}
    { }

    void start()
    {
        read_request();
        check_deadline();
    }

};

stats_server::stats_server( stats_server_cfg const& cfg
                          , worker const& w
                          , int i
                          , net::io_context& ioc)
: acceptor_ {ioc, { net::ip::tcp::v4()
                  , static_cast<unsigned short>(std::stoi(cfg.port) + i)}}
, socket_ {ioc}
, worker_ {w}
{
   run();
}

void stats_server::on_accept(beast::error_code const& ec)
{
   if (ec) {
      if (ec == net::error::operation_aborted) {
         log( loglevel::info
            , "W{0}/stats: Stopping accepting connections"
            , worker_.get_id());
         return;
      }

      log( loglevel::debug, "W{0}/stats: on_accept: {1}"
         , ec.message(), worker_.get_id());
   } else {
      std::make_shared<http_session>( std::move(socket_)
                                    , worker_)->start();
   }

   do_accept();
}

void stats_server::do_accept()
{
   auto handler = [this](beast::error_code const& ec)
      { on_accept(ec); };

   acceptor_.async_accept(socket_, handler);
}

void stats_server::run()
{
   do_accept();
}

void stats_server::shutdown()
{
   acceptor_.cancel();
}

}

