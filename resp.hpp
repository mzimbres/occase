#pragma once

#include <vector>
#include <string>
#include <iterator>
#include <initializer_list>

namespace rt::redis
{

// Enum of redis commands in one-to-one correspondence with redis
// documentation.
enum class cmd
{ get
, incrby
, lpop
, lrange
, ping
, rpush
, publish
, set
, subscribe
, unsubscribe
};

inline
char const* get_redis_cmd_as_str(cmd c)
{
   switch (c) {
      case cmd::get:         return "GET";
      case cmd::incrby:      return "INCRBY";
      case cmd::lpop:        return "LPOP";
      case cmd::lrange:      return "LRANGE";
      case cmd::ping:        return "PING";
      case cmd::rpush:       return "RPUSH";
      case cmd::publish:     return "PUBLISH";
      case cmd::set:         return "SET";
      case cmd::subscribe:   return "SUBSCRIBE";
      case cmd::unsubscribe: return "UNSUBSCRIBE";
      default: return "";
   }
}

inline
std::string get_bulky_str(std::string const& param)
{
   return "$"
        + std::to_string(std::size(param))
        + "\r\n"
        + param
        + "\r\n";
}

// Assembles strings into a redis command (in resp format).
inline std::string
gen_resp_cmd(cmd c, std::initializer_list<std::string> param)
{
   std::string payload = "*";
   payload += std::to_string(std::size(param) + 1);
   payload += "\r\n";

   payload += get_bulky_str(get_redis_cmd_as_str(c));

   for (auto const& o : param)
      payload += get_bulky_str(o);

   return payload;
}

}

