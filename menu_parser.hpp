#pragma once

#include <stack>
#include <string>

#include "json_utils.hpp"

json gen_location_menu();

std::stack<std::string>
parse_menu_json(json menu, user_bind bind, std::string prefix);

std::vector<json> json_patches(json menu);

