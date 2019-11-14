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

inline
int to_syslog_prio(loglevel ll)
{
   switch (ll)
   {
      case loglevel::emerg:    return LOG_EMERG;
      case loglevel::alert:    return LOG_ALERT;
      case loglevel::crit:     return LOG_CRIT;
      case loglevel::err:      return LOG_ERR;
      case loglevel::warning:  return LOG_WARNING;
      case loglevel::notice:   return LOG_NOTICE;
      case loglevel::info:     return LOG_INFO;
      case loglevel::debug:    return LOG_DEBUG;
      default:
      {
         assert(false);
         return -1;
      }
   }
}

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

