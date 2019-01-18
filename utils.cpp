#include "utils.hpp"

#include <iostream>

#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>

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

   std::cout << "Current values: " << std::endl;
   std::cout << "Soft: " << rl.rlim_cur << std::endl;
   std::cout << "Hard: " << rl.rlim_cur << std::endl;

   // Let us raise our limits.
   rl.rlim_cur = fds;
   rl.rlim_max = fds;

   std::cout << "Seting new values" << std::endl;
   auto const r2 = setrlimit(RLIMIT_NOFILE, &rl);
   if (r2 == -1) {
      perror(nullptr);
      return;
   }

   std::cout << "New values: " << std::endl;
   std::cout << "Soft: " << rl.rlim_cur << std::endl;
   std::cout << "Hard: " << rl.rlim_cur << std::endl;
}

} // rt

