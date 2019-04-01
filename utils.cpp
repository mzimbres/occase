#include "utils.hpp"

#include <iostream>
#include <cassert>

#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fmt/format.h>
#include <string.h>
#include <syslog.h>

#include "utils.hpp"

namespace rt
{

void set_fd_limits(int fds)
{
   rlimit rl;
   rl.rlim_cur = 0;
   rl.rlim_max = 0;

   auto const r1 = getrlimit(RLIMIT_NOFILE, &rl);
   if (r1 == -1) {
      perror(nullptr);
      return;
   }

   auto const* fmt1 = "getrlimit (soft, hard): ({0}, {1})";

   log( fmt::format(fmt1, rl.rlim_cur, rl.rlim_cur)
      , loglevel::info);

   // Let us raise our limits.
   rl.rlim_cur = fds;
   rl.rlim_max = fds;

   auto const r2 = setrlimit(RLIMIT_NOFILE, &rl);
   if (r2 == -1) {
      auto const* fmt3 = "Unable to raise fd limits: {0}";
      log(fmt::format(fmt3, strerror(errno)), loglevel::err);
      return;
   }

   auto const* fmt2 =
      "getrlimit (soft, hard): ({0}, {1})";

   log( fmt::format(fmt2, rl.rlim_cur, rl.rlim_cur)
      , loglevel::info);
}

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

void log(std::string const& msg, loglevel ll)
{
   auto const prio = convert_to_prio(ll);
   syslog(prio, msg.data());
}

void log(char const* msg, loglevel ll)
{
   auto const prio = convert_to_prio(ll);
   syslog(prio, msg);
}

} // rt

