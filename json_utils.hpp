#pragma once

#include <ostream>
#include <string>

#include "config.hpp"

struct group_info {
   std::string title;
   std::string description;
};

struct group_bind {
   std::string host;
   int index;
};

inline
auto operator<(group_bind const& a, group_bind const& b)
{
   return a.index < b.index;
}

struct user_bind {
   std::string tel;
   std::string host;
   int index = -1;
};

std::ostream& operator<<(std::ostream& os, group_info const& o);
std::ostream& operator<<(std::ostream& os, group_bind const& o);
std::ostream& operator<<(std::ostream& os, user_bind const& o);

void to_json(json& j, const group_info& g);
void to_json(json& j, const group_bind& g);
void to_json(json& j, const user_bind& g);

void from_json(const json& j, group_info& g);
void from_json(const json& j, group_bind& g);
void from_json(const json& j, user_bind& g);

