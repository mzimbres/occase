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

void set_fd_limits(int fds);

class logger {
   std::string const indent;
public:
   logger(std::string indent_, bool log_on_stderr);
   ~logger();
};

enum class loglevel
{ emerg
, alert
, crit
, err
, warning
, notice
, info
, debug
};

void log(std::string const& msg, loglevel ll);
void log(char const* msg, loglevel ll);
}

