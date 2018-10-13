#include "resp.hpp"

#include <iostream>
#include <iterator>

namespace aedis
{

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
   switch (*begin++) {
   case '+':
   {
      auto p = begin;
      while (*p != '\r')
         ++p;

      auto const n = static_cast<std::size_t>(std::distance(begin, p));
      std::string_view v {&*begin, n};
      std::cout << v << "\n";
   }
   break;
   case '-':
   {
      auto p = begin;
      while (*p != ' ' && *p != '\r')
         ++p;

      if (*p == '\r') { // Redis bug?
         std::cout << "Redis bug." << std::endl;
         return;
      }

      auto const n = static_cast<std::size_t>(std::distance(begin, p));
      std::string_view error_type {&*begin, n};
      std::cout << "Error type: " << error_type << std::endl;

      ++p;

      begin = p;
      while (*p != '\r')
         ++p;

      auto const d = static_cast<std::size_t>(std::distance(begin, p));
      std::string_view error_msg {&*begin, d};
      std::cout << "Error msg: " << error_msg << std::endl;
   }
   break;
   case ':':
   {
      auto p = begin;
      while (*p != '\r')
         ++p;

      auto const d = static_cast<std::size_t>(std::distance(begin, p));
      std::string_view n {&*begin, d};
      std::cout << n << std::endl;
   }
   break;
   case '$':
   {
      std::cout << "Bulky string." << std::endl;
   }
   break;
   case '*':
   {
      auto len = 0;
      auto p = begin;
      while(*p != '\r') {
          len = (10 * len) + (*p - '0');
          p++;
      }

      std::cout << "Array with size: " << len << std::endl;
   }
   break;
   }

   std::string_view v {str.data(), std::size(str)};
   std::cout << v << "\n";
}

}

