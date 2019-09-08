#pragma once

#include <iterator>

namespace rt
{

template <class C>
auto ssize(C const& c)
{
   auto const size = std::size(c);
   return static_cast<int>(size);
}

// Splits a string in the form ip:port in two strings.
std::pair<std::string, std::string> split(std::string data);

}

