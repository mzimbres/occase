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

struct group_info {
   std::vector<std::string> header;
   std::string hash;
};

std::ostream& operator<<(std::ostream& os, user_bind const& o);

void to_json(json& j, const user_bind& g);
void to_json(json& j, const group_info& g);

void from_json(const json& j, user_bind& g);
void from_json(const json& j, group_info& g);

