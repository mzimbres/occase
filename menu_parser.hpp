#pragma once

#include <stack>
#include <string>

#include "json_utils.hpp"

json gen_location_menu();

// Returns a vector with patches that will add hashes to all leaf
// nodes in the menu.
std::vector<json> gen_hash_patches(json menu);

// Generates a vector with all create_group command for a given menu.
// The menu must have already be hashfied by gen_hash_patches.
std::vector<std::string> gen_create_groups(json menu, user_bind bind);

json gen_group_info(json menu);

