#pragma once

#include <iterator>

#include <fmt/format.h>

#include <syslog.h>

namespace rt
{

template <class C>
auto ssize(C const& c)
{
   auto const size = std::size(c);
   return static_cast<int>(size);
}

void set_fd_limits(int fds);

}

