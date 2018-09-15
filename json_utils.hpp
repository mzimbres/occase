#pragma once

#include <ostream>
#include <string>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

struct user_bind {
   std::string tel;
   std::string host;
   int index = -1;
};

std::ostream& operator<<(std::ostream& os, user_bind const& o);

void to_json(json& j, const user_bind& g);

void from_json(const json& j, user_bind& g);

