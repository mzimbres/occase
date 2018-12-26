#pragma once

#include <string>

namespace rt
{

struct fipe_op {
   std::string tipo;
   int indentation;
};

std::string fipe_dump(std::string const& str, fipe_op const& op);

}

