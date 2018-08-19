#pragma once

#include <ostream>

#include "config.hpp"

struct group_info {
   std::string title;
   std::string description;
};

std::ostream& operator<<(std::ostream& os, group_info const& o);


void to_json(json& j, const group_info& g);
void from_json(const json& j, group_info& g);

