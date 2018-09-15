#pragma once

#include <stack>
#include <string>

#include "json_utils.hpp"

std::string gen_menu_json(int indentation);

std::stack<std::string> parse_menu_json(std::string menu, user_bind bind);

