#include "resp.hpp"

#include <iostream>
#include <iterator>

namespace aedis
{

auto get_data_end(resp_response::const_iterator p)
{
   while (*p != '\r')
      ++p;

   return p;
}

auto handle_other(resp_response::const_iterator begin)
{
   switch (*begin++) {
      case '+': return get_data_end(begin);
      case '-': return get_data_end(begin);
      case ':': return get_data_end(begin);
      case '$': return get_data_end(begin);
   }

   return get_data_end(begin);
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

   auto begin = std::cbegin(str);
   if (*begin == '*') {
      auto len = 0;
      auto p = ++begin;
      while (*p != '\r') {
          len = (10 * len) + (*p - '0');
          p++;
      }

      std::cout << "Array with size: " << len << std::endl;

      std::string_view v {str.data(), std::size(str)};
      std::cout << v << "\n";
      return;
   }

   auto const p = handle_other(begin++);

   auto const n = static_cast<std::size_t>(std::distance(begin, p));
   std::string_view v2 {&*begin, n};
   std::cout << v2 << "\n";

   std::string_view v {str.data(), std::size(str)};
   std::cout << v << "\n";
}

}

