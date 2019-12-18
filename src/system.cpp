#include "system.hpp"

#include <fstream>
#include <cassert>
#include <iostream>

#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "logger.hpp"

namespace occase
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

   log::write( log::level::info
             , "getrlimit (soft, hard): ({0}, {1})"
             , rl.rlim_cur
             , rl.rlim_cur);

   // Let us raise the limits.
   rl.rlim_cur = fds;
   rl.rlim_max = fds;

   auto const r2 = setrlimit(RLIMIT_NOFILE, &rl);
   if (r2 == -1) {
      log::write( log::level::err
                , "Unable to raise fd limits: {0}"
                , strerror(errno));
      return;
   }

   log::write( log::level::info
             , "getrlimit (soft, hard): ({0}, {1})"
             , rl.rlim_cur, rl.rlim_cur);
}

}

