#pragma once

#include <fmt/format.h>

#include <syslog.h>

namespace rt
{

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

inline
int convert_to_prio(loglevel ll)
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

template <class... Args>
void log(loglevel ll, char const* fmt, Args const& ... args)
{
   auto const prio = convert_to_prio(ll);
   syslog(prio, fmt::format(fmt, args...).data());
}

}

