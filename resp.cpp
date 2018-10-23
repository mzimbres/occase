#include "resp.hpp"

#include <iostream>
#include <iterator>

namespace aedis
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

void add_bulky_str(std::string& payload, std::string const& param)
{
   payload += "$";
   payload += std::to_string(std::size(param));
   payload += "\r\n";
   payload += param;
   payload += "\r\n";
}

std::string gen_resp_cmd(std::string cmd, std::vector<std::string> param)
{
   std::string payload = "*";
   payload += std::to_string(std::size(param) + 1);
   payload += "\r\n";

   add_bulky_str(payload, cmd);

   for (auto const& o : param)
      add_bulky_str(payload, o);

   return payload;
}

}

