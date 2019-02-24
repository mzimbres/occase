#include "resp.hpp"

#include <iostream>
#include <iterator>

namespace rt::redis
{

char const* get_redis_cmd_as_str(command c)
{
   switch (c) {
      case command::get:         return "GET";
      case command::incrby:      return "INCRBY";
      case command::lpop:        return "LPOP";
      case command::lrange:      return "LRANGE";
      case command::ping:        return "PING";
      case command::rpush:       return "RPUSH";
      case command::publish:     return "PUBLISH";
      case command::set:         return "SET";
      case command::subscribe:   return "SUBSCRIBE";
      case command::unsubscribe: return "UNSUBSCRIBE";
      default: return "";
   }
}

std::string get_bulky_str(std::string const& param)
{
   return "$"
        + std::to_string(std::size(param))
        + "\r\n"
        + param
        + "\r\n";
}

std::string
gen_resp_cmd(command c, std::initializer_list<std::string> param)
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

