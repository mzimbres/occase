#include "utils.hpp"

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

pidfile_mgr::pidfile_mgr(std::string const& pidfile)
: pidfile_ {pidfile}
{
   FILE *fp = fopen(pidfile_.data(), "w");
   if (fp) {
      log(loglevel::info, "Creating pid file {0}", pidfile_);
      fprintf(fp,"%d\n",(int) getpid());
      fclose(fp);
   } else {
      log(loglevel::info, "Unable to create pidfile");
   }
}

pidfile_mgr::~pidfile_mgr()
{
   if (unlink(pidfile_.data()) == 0) {
      log( loglevel::info
         , "Pid file has been successfully removed: {0}"
         , pidfile_);
   }
}

void daemonize()
{
   if (fork() != 0)
      exit(0);

   setsid();

   int fd;
   if ((fd = open("/dev/null", O_RDWR, 0)) != -1) {
      dup2(fd, STDIN_FILENO);
      dup2(fd, STDOUT_FILENO);
      dup2(fd, STDERR_FILENO);
      if (fd > STDERR_FILENO)
         close(fd);
   }
}

} // rt

