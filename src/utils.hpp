#pragma once

#include <iterator>

namespace occase
{

template <class C>
auto ssize(C const& c)
{
   auto const size = std::size(c);
   return static_cast<int>(size);
}

}

