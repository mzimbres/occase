#pragma once

#include <ostream>
#include <string>

#include "config.hpp"

struct group_info {
   std::string city;
   std::string part;
};

struct user_bind {
   std::string tel;
   std::string host;
   int index = -1;
};

std::ostream& operator<<(std::ostream& os, group_info const& o);
std::ostream& operator<<(std::ostream& os, user_bind const& o);

void to_json(json& j, const group_info& g);
void to_json(json& j, const user_bind& g);

void from_json(const json& j, group_info& g);
void from_json(const json& j, user_bind& g);

