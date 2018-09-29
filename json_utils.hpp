#pragma once

#include <ostream>
#include <string>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

using user_bind = std::string;

struct group_info {
   std::vector<std::string> header;
   std::string hash;
};

void to_json(json& j, const group_info& g);

void from_json(json const& j, group_info& g);

