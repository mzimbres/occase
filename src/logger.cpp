#include "logger.hpp"

#include <iostream>
#include <cassert>

#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <string.h>

#include "utils.hpp"

namespace rt
{

logger::logger(std::string indent_, bool log_on_stderr)
: indent {indent_}
{
   auto option = LOG_PID | LOG_NDELAY | LOG_NOWAIT;
   if (log_on_stderr)
      option |= LOG_PERROR;

   openlog(indent.data(), option, LOG_LOCAL0);
}

logger::~logger()
{
   closelog();
}

namespace global
{
loglevel logfilter = loglevel::notice;
}

loglevel to_loglevel(std::string const& ll)
{
   if (ll == "emerg")   return loglevel::emerg;
   if (ll == "alert")   return loglevel::alert;
   if (ll == "crit")    return loglevel::crit;
   if (ll == "err")     return loglevel::err;
   if (ll == "warning") return loglevel::warning;
   if (ll == "notice")  return loglevel::notice;
   if (ll == "info")    return loglevel::info;
   if (ll == "debug")   return loglevel::debug;

   return loglevel::debug;
}

void log_upto(std::string const& ll)
{
   global::logfilter = to_loglevel(ll);
}

} // rt


