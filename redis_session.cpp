#include <deque>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include <boost/asio.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <fmt/format.h>

#include "utils.hpp"
#include "config.hpp"
#include "redis_session.hpp"

namespace rt::redis
{

session::session(session_cfg cf_, net::io_context& ioc, std::string id_)
: id(id_)
, cfg {cf_}
, resolver {ioc} 
, socket {ioc}
, timer {ioc, std::chrono::steady_clock::time_point::max()}
{
   if (cfg.max_pipeline_size < 1)
      throw std::runtime_error("redis::session: Invalid max pipeline size.");
}

void session::on_resolve( boost::system::error_code const& ec
                        , net::ip::tcp::resolver::results_type results)
{
   if (ec) {
      log( loglevel::warning
         , "{0}/on_resolve: {1}."
         , id, ec.message());

      return;
   }

   auto handler = [this](auto ec, auto iter)
      { on_connect(ec, iter); };

   net::async_connect(socket, results, handler);
}

void session::run()
{
   auto handler = [this](auto ec, auto res)
      { on_resolve(ec, res); };

   resolver.async_resolve(cfg.host, cfg.port, handler);
}

void session::send(std::string msg)
{
   auto const max_pp_size_reached =
      pipeline_counter >= cfg.max_pipeline_size;

   if (max_pp_size_reached) {
      pipeline_counter = 0;
   }

   auto const is_empty = std::empty(msg_queue);

   if (is_empty || std::size(msg_queue) == 1 || max_pp_size_reached) {
      msg_queue.push(std::move(msg));
   } else {
      msg_queue.back() += msg; // Uses pipeline.
      ++pipeline_counter;
   }

   if (is_empty && socket.is_open())
      net::async_write( socket, net::buffer(msg_queue.front())
                      , [this](auto ec, auto n) {on_write(ec, n);});
}

void session::close()
{
   boost::system::error_code ec;
   socket.shutdown(net::ip::tcp::socket::shutdown_send, ec);
   if (ec) {
      log( loglevel::warning
         , "{0}/close: {1}."
         , id, ec.message());
   }

   ec = {};
   socket.close(ec);
   if (ec) {
      log( loglevel::warning
         , "{0}/close: {1}."
         , id, ec.message());
   }

   timer.cancel();
}

void session::start_reading_resp()
{
   buffer.res.clear();

   auto handler = [this](auto const& ec)
      { on_resp(ec); };

   async_read_resp(socket, &buffer, handler);
}

void session::on_connect( boost::system::error_code const& ec
                        , net::ip::tcp::endpoint const& endpoint)
{
   if (ec) {
      // TODO: Traverse all endpoints if this one is not available. If
      // none is available should we continue?
      return;
   }

   start_reading_resp();

   // Consumes any messages that have been eventually posted while the
   // connection was not established.
   if (!std::empty(msg_queue)) {
      auto handler = [this](auto ec, auto n)
         { on_write(ec, n); };

      net::async_write( socket, net::buffer(msg_queue.front())
                      , handler);
   }

   // Calls user callback to inform a successfull connect to redis.
   // It may wish to start sending some command.
   //
   // Since this callback may call the send member function on this
   // object, we have to call it AFTER the write operation above,
   // otherwise the message will be sent twice.
   on_conn_handler();
}

void session::on_resp(boost::system::error_code const& ec)
{
   if (ec) {
      if (ec == net::error::eof) {
         // Redis has cleanly closed the connection. We try a reconnect.
         timer.expires_after(cfg.conn_retry_interval);

         auto const handler = [this](auto ec)
         {
            if (ec) {
               if (ec == net::error::operation_aborted) {
                  // The timer has been canceled. Probably somebody
                  // shutting down the application while we are trying to
                  // reconnect.
                  return;
               }

               log( loglevel::warning
                  , "{0}/on_resp1: {1}."
                  , id, ec.message());

               return;
            }

            // Given that the peer has shutdown the connection (I think)
            // we do not need to call shutdown.
            //socket.shutdown(net::ip::tcp::socket::shutdown_both, ec);
            socket.close(ec);
            assert(!socket.is_open());

            // Instead of simply trying to reconnect I will run the
            // resolver again.
            std::cout << "Trying to reconnect." << std::endl;
            run();
         };

         timer.async_wait(handler);
         return;
      }

      log( loglevel::warning
         , "{0}/on_resp2: {1}."
         , id, ec.message());
      return;
   }

   on_msg_handler(ec, std::move(buffer.res));

   start_reading_resp();

   if (std::empty(msg_queue))
      return;

   msg_queue.pop();

   if (std::empty(msg_queue))
      return;

   auto handler = [this](auto ec, auto n)
      { on_write(ec, n); };

   net::async_write(socket, net::buffer(msg_queue.front()), handler);
}

void session::on_write(boost::system::error_code ec, std::size_t n)
{
   if (ec) {
      log( loglevel::info, "{0}/on_write: {1}."
         , id, ec.message());
      return;
   }
}

}

