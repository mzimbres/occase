#pragma once

#include <stack>
#include <string>

#include "json_utils.hpp"

namespace rt
{

json gen_location_menu();
json gen_sim_menu();

// Generates a vector with all create_group command for a given menu.
std::vector<std::string> gen_create_groups(json menu);

json gen_group_info(json menu);

// Puts all group hashes into a vector.
std::vector<std::string> get_hashes(json menu);

std::string to_str(int i);

}

