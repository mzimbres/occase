#pragma once

#include <string>

namespace rt
{

std::string
fipe_dump( std::string const& str
         , int indentation
         , std::string const& tipo
         , char c
         , int split_idx
         , int filter_idx
         , char sep);
}

