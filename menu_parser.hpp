#pragma once

#include <string>
#include <stack>

#include "config.hpp"
#include "json_utils.hpp"

std::string gen_menu_json();

std::stack<std::string> parse_menu_json(std::string menu, user_bind bind);

