#pragma once

#include <array>
#include <queue>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numeric>
#include <iostream>
#include <functional>
#include <type_traits>

#include <boost/asio.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>

#include <fmt/format.h>

#include "net.hpp"
#include "utils.hpp"
#include "logger.hpp"

namespace aedis
{

struct session_cfg {
   std::string host;
   std::string port;

   // A list of redis sentinels e.g. ip1:port1 ip2:port2 ...
   std::vector<std::string> sentinels;

   std::chrono::milliseconds conn_retry_interval {500};

   // We have to restrict the length of the pipeline to not block
   // redis for long periods.
   int max_pipeline_size {10000};
};

namespace priv
{

struct resp_buffer {
   std::string data;
   std::vector<std::string> res;
};

inline
std::string make_bulky_item(std::string const& param)
{
   auto const s = std::size(param);
   return "$"
        + std::to_string(s)
        + "\r\n"
        + param
        + "\r\n";
}

inline
std::string make_cmd_header(int size)
{
   return "*" + std::to_string(size) + "\r\n";
}

struct accumulator {
   auto operator()(std::string a, std::string b) const
   {
      a += make_bulky_item(b);
      return a;
   }
};

template <class Iter>
auto resp_assemble(char const* cmd, Iter begin, Iter end)
{
   auto const d = std::distance(begin, end);
   auto payload = make_cmd_header(d + 1) + make_bulky_item(cmd);
   return std::accumulate(begin , end, std::move(payload), accumulator{});
}

inline
auto resp_assemble(char const* cmd)
{
   std::initializer_list<std::string> arg;
   return resp_assemble(cmd, std::begin(arg), std::end(arg));
}

inline
auto resp_assemble(char const* cmd, std::string const& str)
{
   auto arg = {str};
   return resp_assemble(cmd, std::begin(arg), std::end(arg));
}

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

template <class Iter>
auto rpush(std::string const& key, Iter begin, Iter end)
{
   auto const d = std::distance(begin, end);

   auto payload = priv::make_cmd_header(2 + d)
                + priv::make_bulky_item("RPUSH")
                + priv::make_bulky_item(key);

   auto cmd_str = std::accumulate( begin
                                 , end
                                 , std::move(payload)
                                 , priv::accumulator{});
   return cmd_str;
}

template <class Iter>
auto lpush(std::string const& key, Iter begin, Iter end)
{
   auto const d = std::distance(begin, end);

   auto payload = priv::make_cmd_header(2 + d)
                + priv::make_bulky_item("LPUSH")
                + priv::make_bulky_item(key);

   auto cmd_str = std::accumulate( begin
                                 , end
                                 , std::move(payload)
                                 , priv::accumulator{});
   return cmd_str;
}

inline
auto multi()
{
   return priv::resp_assemble("MULTI");
}

inline
auto exec()
{
   return priv::resp_assemble("EXEC");
}

inline
auto incr(std::string const& key)
{
   return priv::resp_assemble("INCR", key);
}

inline
auto lpop(std::string const& key)
{
   return priv::resp_assemble("LPOP", key);
}

inline
auto subscribe(std::string const& key)
{
   return priv::resp_assemble("SUBSCRIBE", key);
}

inline
auto unsubscribe(std::string const& key)
{
   return priv::resp_assemble("UNSUBSCRIBE", key);
}

inline
auto get(std::string const& key)
{
   return priv::resp_assemble("GET", key);
}

inline
auto publish(std::string const& key, std::string const& msg)
{
   auto par = {key, msg};
   return priv::resp_assemble("PUBLISH", std::begin(par), std::end(par));
}

inline
auto set(std::string const& key, std::string const& value)
{
   auto par = {key, value};
   return priv::resp_assemble("SET", std::begin(par), std::end(par));
}

inline
auto hset(std::initializer_list<std::string const> const& l)
{
   return priv::resp_assemble("HSET", std::begin(l), std::end(l));
}

inline
auto hget(std::string const& key, std::string const& field)
{
   auto par = {key, field};
   return priv::resp_assemble("HGET", std::begin(par), std::end(par));
}

inline
auto hmget( std::string const& key
          , std::string const& field1
          , std::string const& field2)
{
   auto par = {key, field1, field2};
   return priv::resp_assemble("HMGET", std::begin(par), std::end(par));
}

inline
auto expire(std::string const& key, int secs)
{
   auto par = {key, std::to_string(secs)};
   return priv::resp_assemble("EXPIRE", std::begin(par), std::end(par));
}

inline
auto zadd(std::string const& key, int score, std::string const& value)
{
   auto par = {key, std::to_string(score), value};
   return priv::resp_assemble("ZADD", std::begin(par), std::end(par));
}

inline
auto zrange(std::string const& key, int min, int max)
{
   auto par = { key
              , std::to_string(min)
              , std::to_string(max)
              };

   return priv::resp_assemble("zrange", std::begin(par), std::end(par));
}

inline
auto zrangebyscore(std::string const& key, int min, int max)
{
   auto max_str = std::string {"inf"};
   if (max != -1)
      max_str = std::to_string(max);

   auto par = { key
              , std::to_string(min)
              , max_str
              //, std::string {"withscores"}
              };

   return priv::resp_assemble("zrangebyscore", std::begin(par), std::end(par));
}

inline
auto zremrangebyscore(std::string const& key, int score)
{
   auto const s = std::to_string(score);
   auto par = {key, s, s};
   return priv::resp_assemble("ZREMRANGEBYSCORE", std::begin(par), std::end(par));
}

inline
auto lrange(std::string const& key, int min, int max)
{
   auto par = { key
              , std::to_string(min)
              , std::to_string(max)
              };

   return priv::resp_assemble("lrange", std::begin(par), std::end(par));
}

inline
auto del(std::string const& key)
{
   auto par = {key};
   return priv::resp_assemble("del", std::begin(par), std::end(par));
}

inline
auto llen(std::string const& key)
{
   auto par = {key};
   return priv::resp_assemble("llen", std::begin(par), std::end(par));
}

class session {
public:
   using on_conn_handler_type = std::function<void()>;

   using on_disconnect_handler_type =
      std::function<void(boost::system::error_code const&)>;

   using msg_handler_type =
      std::function<void( boost::system::error_code const&
                        , std::vector<std::string>)>;

private:
   std::string id;
   session_cfg cfg;
   net::ip::tcp::resolver resolver;
   net::ip::tcp::socket socket;
   net::steady_timer timer;
   priv::resp_buffer buffer;
   std::queue<std::string> msg_queue;
   int pipeline_counter = 0;

   msg_handler_type on_msg_handler = [](auto const&, auto) {};
   on_conn_handler_type on_conn_handler = [](){};
   on_disconnect_handler_type on_disconnect_handler = [](auto const&){};

   void start_reading_resp()
   {
      buffer.res.clear();

      auto handler = [this](auto const& ec)
         { on_resp(ec); };

      priv::async_read_resp(socket, &buffer, handler);
   }

   void on_resolve( boost::system::error_code const& ec
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

   void on_connect( boost::system::error_code const& ec
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

   void on_resp(boost::system::error_code const& ec)
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

   void on_write(boost::system::error_code ec, std::size_t n)
   {
      if (ec) {
         log( rt::loglevel::info, "{0}/on_write: {1}."
            , id, ec.message());
         return;
      }
   }

   void do_write()
   {
      auto handler = [this](auto ec, auto n)
         { on_write(ec, n); };

      net::async_write(socket, net::buffer(msg_queue.front()), handler);
   }

   void on_conn_closed(boost::system::error_code ec)
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

public:
   session(session_cfg cfg_, net::io_context& ioc, std::string id_)
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

   void set_on_conn_handler(on_conn_handler_type handler)
      { on_conn_handler = std::move(handler);};

   void set_on_disconnect_handler(on_disconnect_handler_type handler)
      { on_disconnect_handler = std::move(handler);};

   void set_msg_handler(msg_handler_type handler)
      { on_msg_handler = std::move(handler);};

   void send(std::string msg)
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

   void close()
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


   void run()
   {
      //auto addr = split(cfg.sentinels.front());
      //std::cout << addr.first << " -- " << addr.second << std::endl;

      // Calling sync resolve to avoid starting a new thread.
      boost::system::error_code ec;
      auto res = resolver.resolve(cfg.host, cfg.port, ec);
      on_resolve(ec, res);
   }
};

}

