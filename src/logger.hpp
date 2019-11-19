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

template <class T>
T to_loglevel(std::string const& ll)
{
   if (ll == "emerg")   return T::emerg;
   if (ll == "alert")   return T::alert;
   if (ll == "crit")    return T::crit;
   if (ll == "err")     return T::err;
   if (ll == "warning") return T::warning;
   if (ll == "notice")  return T::notice;
   if (ll == "info")    return T::info;
   if (ll == "debug")   return T::debug;

   return T::debug;
}

void log_upto(loglevel ll);

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

