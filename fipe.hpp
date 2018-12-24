#pragma once

#include <string>

namespace rt
{

struct fipe_op {
   std::string file;
   std::string tipo;
};

void fipe_dump(fipe_op const& op);

}

