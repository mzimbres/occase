#pragma once

#include <stack>
#include <string>

#include "json_utils.hpp"

namespace rt
{

json gen_location_menu();
json gen_sim_menu(int l);

// Puts all group hashes into a vector.
std::vector<std::string> get_hashes(json menu);

std::string to_str(int i);

}

