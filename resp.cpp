#include "resp.hpp"

#include <iostream>
#include <iterator>

namespace aedis
{

int get_length(std::string::const_iterator& p)
{
   auto len = 0;
   while (*p != '\r') {
       len = (10 * len) + (*p - '0');
       p++;
   }
   return len;
}

auto get_data_end(resp_response::const_iterator p)
{
   while (*p != '\r')
      ++p;

   return p;
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

auto handle_other(resp_response::const_iterator begin)
{
   auto const c = *begin;
   auto const p = get_data_end(++begin);
   auto const n = static_cast<std::size_t>(std::distance(begin, p));

   switch (c) {
      case '+':
      {
         std::cout << std::string_view {&*begin, n} << "\n";
      }
      break;
      case '-':
      {
         std::cout << std::string_view {&*begin, n} << "\n";
      }
      break;
      case ':':
      {
         auto const s = std::string {begin, begin + n};
         std::cout << s << "\n";
      }
      break;
      case '$':
      {
         auto const s = std::string {begin, begin + n};
         auto const size = std::stoi(s);

         std::string ss {p + 2, p + 2 + size};
         std::cout << "Bulky string: " << ss << "\n";
         return p + 2 + size;
      }
      break;
   }

   return p + 2;
}

std::string get_simple_string(std::string const& str)
{
   if (str.front() != '+')
      throw std::runtime_error("get_simple_string: Not a string.");

   auto const p = std::next(std::cbegin(str));
   return std::string {p, get_data_end(p)};
}

std::string get_int(std::string const& str)
{
   if (str.front() != ':')
      throw std::runtime_error("get_int: Not an integer.");

   auto const p = std::next(std::cbegin(str));
   return std::string {p, get_data_end(p)};
}

std::string get_bulky_string(std::string const& str)
{
   if (str.front() != '$')
      throw std::runtime_error("get_bulky_string: Not a bulky string.");

   // TODO: Check boundaries.
   auto p = std::next(std::cbegin(str));
   auto const l = get_length(p);
   return std::string {p + 2, p + 2 + l};
}

void resp_response::process_response() const
{
   //auto const end = std::cend(str);
   auto begin = std::cbegin(str);
   if (*begin == '*') {
      ++begin;
      auto len = get_length(begin);
      std::cout << "Array with size: " << len << std::endl;

      for (auto i = 0; i < len; ++i) {
         begin = handle_other(begin + 2);
      }

      std::string_view v {str.data(), std::size(str)};
      std::cout << v << "\n";
      return;
   }

   handle_other(begin);

   std::string_view v {str.data(), std::size(str)};
   std::cout << v << "\n";
}

}

