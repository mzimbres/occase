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

} // rt


