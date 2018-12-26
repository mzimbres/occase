#pragma once

#include <string>

namespace rt
{

struct fipe_op {
   std::string file;
   std::string tipo;
   int indentation;
};

std::string fipe_dump(fipe_op const& op);

}

