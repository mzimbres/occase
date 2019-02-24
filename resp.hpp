#pragma once

#include <vector>
#include <string>
#include <numeric>
#include <iterator>
#include <initializer_list>

namespace rt::redis
{

inline
std::string get_bulky_str(std::string param)
{
   auto const s = std::size(param);
   return "$"
        + std::to_string(s)
        + "\r\n"
        + std::move(param)
        + "\r\n";
}

template <class Iter>
auto resp_assemble(char const* c, Iter begin, Iter end)
{
   auto const d = std::distance(begin, end);
   std::string payload = "*";
   payload += std::to_string(d + 1);
   payload += "\r\n";
   payload += get_bulky_str(c);

   auto const op = [](auto a, auto b)
   { return std::move(a) + get_bulky_str(std::move(b)); };

   return std::accumulate(begin , end, std::move(payload), op);
}

}

