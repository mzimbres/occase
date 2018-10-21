#include "resp.hpp"

#include <iostream>
#include <iterator>

namespace aedis
{

std::size_t get_length(char const*& p)
{
   std::size_t len = 0;
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

std::string_view get_simple_string(char const* begin)
{
   if (*begin != '+')
      throw std::runtime_error("get_simple_string: Not a simple string.");

   auto p = begin;
   while (*++p != '\r');

   auto const n = static_cast<std::size_t>(std::distance(++begin, p));
   return std::string_view {begin, n};
}

std::string_view get_int(char const* begin)
{
   if (*begin != ':')
      throw std::runtime_error("get_int: Not an integer.");

   auto p = ++begin;
   while (*p != '\r')
      ++p;

   auto const d = static_cast<std::size_t>(std::distance(begin, p));
   return std::string_view {begin, d};
}

std::string_view get_bulky_string(char const* begin, std::size_t d)
{
   if (d == 0)
      return {};

   if (*begin != '$')
      throw std::runtime_error("get_bulky_string: Not a bulky string.");

   auto p = std::next(begin);
   auto const l = get_length(p);
   auto const d2 = static_cast<std::size_t>(std::distance(begin, p));
   if (l > d - d2 - 4)
      throw std::runtime_error("get_bulky_string: Inconsistent data.");

   return std::string_view {p + 2, l};
}

std::vector<std::string_view>
   get_bulky_string_array(char const* begin, std::size_t s)
{
   if (*begin != '*')
      throw std::runtime_error("get_bulky_string_array: Inconsistent data.");

   // TODO: Add boundary checking.
   std::vector<std::string_view> ret;
   auto p = std::next(begin);
   auto l = get_length(p);
   p += 2;
   while (l-- != 0) {
      std::string_view v;
      if (*p == '$')
         v = get_bulky_string(p, s);
      else if (*p == ':')
         v = get_int(p);

      p = &v.back() + 3;
      ret.push_back(std::move(v));
   }

   return ret;
}

void resp_response::process_response() const
{
   //auto const end = std::cend(str);
   //auto begin = std::cbegin(str);
   //if (*begin == '*') {
   //   ++begin;
   //   auto len = get_length(begin);
   //   std::cout << "Array with size: " << len << std::endl;

   //   for (auto i = 0; i < len; ++i) {
   //      begin = handle_other(begin + 2);
   //   }

   //   std::string_view v {str.data(), std::size(str)};
   //   std::cout << v << "\n";
   //   return;
   //}

   //handle_other(begin);

   //std::string_view v {str.data(), std::size(str)};
   //std::cout << v << "\n";
}

}

