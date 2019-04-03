#include "utils.hpp"

#include <iostream>
#include <cassert>

#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <string.h>

#include "logger.hpp"

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

   log( loglevel::info
      , "getrlimit (soft, hard): ({0}, {1})"
      , rl.rlim_cur, rl.rlim_cur);

   // Let us raise our limits.
   rl.rlim_cur = fds;
   rl.rlim_max = fds;

   auto const r2 = setrlimit(RLIMIT_NOFILE, &rl);
   if (r2 == -1) {
      log( loglevel::err
         , "Unable to raise fd limits: {0}"
         , strerror(errno));
      return;
   }

   log( loglevel::info
      , "getrlimit (soft, hard): ({0}, {1})"
      , rl.rlim_cur, rl.rlim_cur);
}

} // rt

