#include "logger.hpp"

#include <cassert>

namespace rt
{

namespace global
{
loglevel logfilter = loglevel::notice;
}

void log_upto(loglevel ll)
{
   global::logfilter = ll;
}

} // rt


