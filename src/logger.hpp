#pragma once

#include <iostream>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include <syslog.h>

namespace occase { namespace log {

enum class level
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
T to_level(std::string const& ll)
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

void upto(level ll);

namespace global {extern level filter;}

inline                                                                                                                                                      
auto ignore(level ll)                                                                                                                                
{                                                                                                                                                           
   return ll > global::filter;                                                                                                                           
}

template <class... Args>
void write(level ll, char const* fmt, Args const& ... args)
{
   if (ll > global::filter)
      return;

   std::clog << fmt::format(fmt, args...) << std::endl;
}

}
}

