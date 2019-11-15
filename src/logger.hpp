#pragma once

#include <iostream>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <syslog.h>

namespace rt
{

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

void log_upto(std::string const& ll);

namespace global {extern loglevel logfilter;}

inline                                                                                                                                                      
auto ignore_log(loglevel ll)                                                                                                                                
{                                                                                                                                                           
   return ll > global::logfilter;                                                                                                                           
}

template <class... Args>
void log(loglevel ll, char const* fmt, Args const& ... args)
{
   if (ll > global::logfilter)
      return;

   std::clog << fmt::format(fmt, args...) << std::endl;
}

}

