#pragma once

#include <iterator>

#include <fmt/format.h>

namespace rt
{

template <class C>
auto ssize(C const& c)
{
   auto const size = std::size(c);
   return static_cast<int>(size);
}

void set_fd_limits(int fds);

// Creates and removes the pid file using RAII.
struct pidfile_mgr {
   std::string const pidfile {"/var/run/menu_chat_server.pid"};
   pidfile_mgr();
   ~pidfile_mgr();
};

void daemonize();

}

