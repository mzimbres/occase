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

void add_bulky_str(std::string& payload, std::string const& param)
{
   payload += "$";
   payload += std::to_string(std::size(param));
   payload += "\r\n";
   payload += param;
   payload += "\r\n";
}

std::string
gen_resp_cmd(command c, std::initializer_list<std::string> param)
{
   std::string payload = "*";
   payload += std::to_string(std::size(param) + 1);
   payload += "\r\n";

   add_bulky_str(payload, get_redis_cmd_as_str(c));

   for (auto const& o : param)
      add_bulky_str(payload, o);

   return payload;
}

}

