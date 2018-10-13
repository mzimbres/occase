#include "resp.hpp"

#include <iostream>
#include <iterator>

namespace aedis
{

auto get_length(resp_response::const_iterator& p)
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

void resp_response::process_response() const
{
   // Checks whether the array is well formed.
   if (std::size(str) < 4) {
      std::cout << "Ill formed array." << std::endl;
      return;
   }

   auto const s = std::size(str);
   if (str[s - 2] != '\r' && str[s - 1] != '\n') {
      std::cout << "Ill formed array." << std::endl;
      return;
   }

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

