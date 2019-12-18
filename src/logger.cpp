#include "logger.hpp"

#include <cassert>

namespace occase { namespace log {

namespace global
{
level filter = level::notice;
}

void upto(level ll)
{
   global::filter = ll;
}

}
}


