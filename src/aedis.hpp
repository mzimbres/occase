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

#include "net.hpp"

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

   void start_reading_resp();

   void on_resolve( boost::system::error_code const& ec
                  , net::ip::tcp::resolver::results_type results);
   void on_connect( boost::system::error_code const& ec
                  , net::ip::tcp::endpoint const& endpoint);
   void on_resp(boost::system::error_code const& ec);
   void on_write( boost::system::error_code ec
                , std::size_t n);
   void do_write();
   void on_conn_closed(boost::system::error_code ec);

public:
   session(session_cfg cf_, net::io_context& ioc, std::string id_);

   void set_on_conn_handler(on_conn_handler_type handler)
      { on_conn_handler = std::move(handler);};

   void set_on_disconnect_handler(on_disconnect_handler_type handler)
      { on_disconnect_handler = std::move(handler);};

   void set_msg_handler(msg_handler_type handler)
      { on_msg_handler = std::move(handler);};

   void send(std::string req);
   void close();
   void run();
};

}

