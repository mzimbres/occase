#include "resp.hpp"

#include <iostream>
#include <iterator>

namespace rt
{

std::size_t get_length(char const* p)
{
   std::size_t len = 0;
   while (*p != '\r') {
       len = (10 * len) + (*p - '0');
       p++;
   }
   return len;
}

char const* get_redis_cmd_as_str(redis_cmd cmd)
{
   switch (cmd) {
      case redis_cmd::get:         return "GET";
      case redis_cmd::incrby:      return "INCRBY";
      case redis_cmd::lpop:        return "LPOP";
      case redis_cmd::lrange:      return "LRANGE";
      case redis_cmd::ping:        return "PING";
      case redis_cmd::rpush:       return "RPUSH";
      case redis_cmd::publish:     return "PUBLISH";
      case redis_cmd::set:         return "SET";
      case redis_cmd::subscribe:   return "SUBSCRIBE";
      case redis_cmd::unsubscribe: return "UNSUBSCRIBE";
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

redis_req
gen_resp_cmd( redis_cmd cmd
            , std::initializer_list<std::string> param
            , std::string const& user_id)
{
   std::string payload = "*";
   payload += std::to_string(std::size(param) + 1);
   payload += "\r\n";

   add_bulky_str(payload, get_redis_cmd_as_str(cmd));

   for (auto const& o : param)
      add_bulky_str(payload, o);

   return {cmd, std::move(payload), user_id};
}

}

