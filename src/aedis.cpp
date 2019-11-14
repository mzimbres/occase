#include "aedis.hpp"

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

#include "net.hpp"
#include "utils.hpp"
#include "logger.hpp"

namespace aedis
{

namespace priv
{

// Converts a decimal number in ascii format to integer.
inline
std::size_t get_length(char const* p)
{
   std::size_t len = 0;
   while (*p != '\r') {
       len = (10 * len) + (*p - '0');
       p++;
   }
   return len;
}

template < class AsyncStream
         , class Handler>
struct read_resp_op {
   AsyncStream& stream;
   Handler handler;
   resp_buffer* buffer = nullptr;
   int start = 0;
   int counter = 1;
   bool bulky_str_read = false;

   read_resp_op( AsyncStream& stream_
               , resp_buffer* buffer_
               , Handler handler_)
   : stream(stream_)
   , handler(std::move(handler_))
   , buffer(buffer_)
   { }

   void operator()( boost::system::error_code const& ec, std::size_t n
                  , int start_ = 0)
   {
      switch (start = start_) {
         for (;;) {
            case 1:
            net::async_read_until( stream, net::dynamic_buffer(buffer->data)
                                 , "\r\n", std::move(*this));
            return; default:

            if (ec || n < 3) {
               handler(ec);
               return;
            }

            auto str_flag = false;
            if (bulky_str_read) {
               buffer->res.push_back(buffer->data.substr(0, n - 2));
               --counter;
            } else {
               if (counter != 0) {
                  switch (buffer->data.front()) {
                     case '$':
                     {
                        // TODO: Do not push in the vector but find a way to
                        // report nil.
                        if (buffer->data.compare(1, 2, "-1") == 0) {
                           buffer->res.push_back({});
                           --counter;
                        } else {
                           str_flag = true;
                        }
                     }
                     break;
                     case '+':
                     case '-':
                     case ':':
                     {
                        buffer->res.push_back(buffer->data.substr(1, n - 3));
                        --counter;
                     }
                     break;
                     case '*':
                     {
                        //assert(counter == 1);
                        counter = get_length(buffer->data.data() + 1);
                     }
                     break;
                     default:
                        assert(false);
                  }
               }
            }

            buffer->data.erase(0, n);

            if (counter == 0) {
               handler(boost::system::error_code{});
               return;
            }

            bulky_str_read = str_flag;
         }
      }
   }
};

template < class AsyncReadStream
         , class ReadHandler
         >
inline bool
asio_handler_is_continuation( read_resp_op< AsyncReadStream
                                          , ReadHandler
                                          >* this_handler)
{
   return this_handler->start == 0 ? true
      : boost_asio_handler_cont_helpers::is_continuation(
            this_handler->handler);
}

template < class AsyncStream
         , class CompletionToken>
BOOST_ASIO_INITFN_RESULT_TYPE(CompletionToken, void(boost::beast::error_code))
async_read_resp( AsyncStream& s
               , resp_buffer* buffer
               , CompletionToken&& handler)
{
   using read_handler_signature = void (boost::system::error_code const&);

   net::async_completion< CompletionToken
                        , read_handler_signature
                        > init {handler};

   using handler_type = 
      read_resp_op< AsyncStream
                  , BOOST_ASIO_HANDLER_TYPE( CompletionToken
                                           , read_handler_signature)
                  >;

   handler_type {s, buffer, init.completion_handler}({}, 0, 1);

   return init.result.get();
}

}

session::session(session_cfg cfg_, net::io_context& ioc, std::string id_)
: id(id_)
, cfg {cfg_}
, resolver {ioc} 
, socket {ioc}
, timer {ioc, std::chrono::steady_clock::time_point::max()}
{
   if (cfg.max_pipeline_size < 1)
      throw std::runtime_error("redis::session: Invalid max pipeline size.");

   if (std::empty(cfg_.sentinels))
      throw std::runtime_error("redis::session: No sentinels provided.");
}

void session::on_resolve( boost::system::error_code const& ec
                        , net::ip::tcp::resolver::results_type results)
{
   if (ec) {
      log(rt::loglevel::warning, "{0}/on_resolve: {1}.", id, ec.message());
      return;
   }

   auto handler = [this](auto ec, auto iter)
      { on_connect(ec, iter); };

   net::async_connect(socket, results, handler);
}

void session::run()
{
   //auto addr = split(cfg.sentinels.front());
   //std::cout << addr.first << " -- " << addr.second << std::endl;

   // Calling sync resolve to avoid starting a new thread.
   boost::system::error_code ec;
   auto res = resolver.resolve(cfg.host, cfg.port, ec);
   on_resolve(ec, res);
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
      do_write();
}

void session::close()
{
   boost::system::error_code ec;
   socket.shutdown(net::ip::tcp::socket::shutdown_send, ec);

   if (ec) {
      log(rt::loglevel::warning, "{0}/close: {1}.", id, ec.message());
   }

   ec = {};
   socket.close(ec);
   if (ec) {
      log(rt::loglevel::warning, "{0}/close: {1}.", id, ec.message());
   }

   timer.cancel();
}

void session::start_reading_resp()
{
   buffer.res.clear();

   auto handler = [this](auto const& ec)
      { on_resp(ec); };

   priv::async_read_resp(socket, &buffer, handler);
}

void session::do_write()
{
   auto handler = [this](auto ec, auto n)
      { on_write(ec, n); };

   net::async_write(socket, net::buffer(msg_queue.front()), handler);
}

void session::on_connect( boost::system::error_code const& ec
                        , net::ip::tcp::endpoint const& endpoint)
{
   if (ec) {
      return;
   }

   log( rt::loglevel::info
      , "{0}/on_connect: Success connecting to {1}"
      , id
      , endpoint);

   start_reading_resp();

   // Consumes any messages that have been eventually posted while the
   // connection was not established, or consumes msgs when a
   // connection to redis is restablished.
   if (!std::empty(msg_queue)) {
      log( rt::loglevel::debug
         , "{0}/on_connect: Number of messages {1}"
         , id, std::size(msg_queue));
      do_write();
   }

   // Calls user callback to inform a successfull connect to redis.
   // It may wish to start sending some commands.
   //
   // Since this callback may call the send member function on this
   // object, we have to call it AFTER the write operation above,
   // otherwise the message will be sent twice.
   on_conn_handler();
}

void session::on_conn_closed(boost::system::error_code ec)
{
   if (ec) {
      if (ec == net::error::operation_aborted) {
         // The timer has been canceled. Probably somebody
         // shutting down the application while we are trying to
         // reconnect.
         return;
      }

      log( rt::loglevel::warning, "{0}/on_conn_closed: {1}"
         , id, ec.message());

      return;
   }

   // Given that the peer has shutdown the connection (I think)
   // we do not need to call shutdown.
   //socket.shutdown(net::ip::tcp::socket::shutdown_both, ec);
   socket.close(ec);

   // Instead of simply trying to reconnect I will run the
   // resolver again. This will be changes when sentinel
   // support is implemented.
   run();
}

void session::on_resp(boost::system::error_code const& ec)
{
   if (ec) {
      log(rt::loglevel::warning, "{0}/on_resp1: {1}.", id, ec.message());
      auto const b1 = ec == net::error::eof;
      auto const b2 = ec == net::error::connection_reset;
      if (b1 || b2) {
         // Redis has cleanly closed the connection, we try to
         // reconnect.
         timer.expires_after(cfg.conn_retry_interval);

         auto const handler = [this](auto const& ec)
            { on_conn_closed(ec); };

         timer.async_wait(handler);
         return;
      }

      if (ec == net::error::operation_aborted) {
         // The operation has been canceled, this can happen in only
         // one way
         //
         // 1. There has been a request from the worker to close the
         //    connection and leave. In this case we should NOT try to
         //    reconnect. We have nothing to do.
         return;
      }

      log( rt::loglevel::warning
         , "{0}/on_resp2: Unhandled error '{1}'."
         , id, ec.message());

      return;
   }

   on_msg_handler(ec, std::move(buffer.res));

   start_reading_resp();

   if (std::empty(msg_queue))
      return;

   msg_queue.pop();

   if (!std::empty(msg_queue))
      do_write();
}

void session::on_write(boost::system::error_code ec, std::size_t n)
{
   if (ec) {
      log( rt::loglevel::info, "{0}/on_write: {1}."
         , id, ec.message());
      return;
   }
}

}

