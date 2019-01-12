#pragma once

#include <ostream>
#include <string>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace rt
{

struct menu_elem {
   std::string data;
   unsigned long depth = 0;
   int version = 0;
};

void to_json(json& j, menu_elem const& e);
void from_json(json const& j, menu_elem& e);

}

