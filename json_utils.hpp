#pragma once

#include <ostream>
#include <string>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

struct user_bind {
   std::string tel;
   std::string pwd;
   std::string host;
};

struct group_info {
   std::vector<std::string> header;
   std::string hash;
};

std::ostream& operator<<(std::ostream& os, const user_bind& o);

void to_json(json& j, user_bind const& g);
void to_json(json& j, const group_info& g);

void from_json(json const& j, user_bind& g);
void from_json(json const& j, group_info& g);

