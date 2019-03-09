#pragma once

#include <iterator>

namespace rt
{
   template <class C>
   auto ssize(C const& c)
   {
      auto const size = std::size(c);
      return static_cast<std::ptrdiff_t>(size);
   }

   void set_fd_limits(int fds);
}

