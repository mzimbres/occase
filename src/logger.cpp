#include "logger.hpp"

#include <cassert>

namespace rt
{

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


