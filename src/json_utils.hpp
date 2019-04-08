#pragma once

#include <ostream>
#include <string>

#include <nlohmann/json.hpp>

using json = nlohmann::json;
using menu_code_type = std::vector<std::vector<std::vector<int>>>;

namespace rt
{

struct pub_item {
   int id;
   std::string from;
   std::string msg;
   menu_code_type to;

   friend
   auto operator<(pub_item const& a, pub_item const& b) noexcept
   {
      return a.id < b.id;
   }
};

inline
void to_json(json& j, pub_item const& e)
{
   j = json{{"id", e.id}, {"from", e.from}, {"msg", e.msg}, {"to", e.to}};
}

inline
void from_json(json const& j, pub_item& e)
{
  e.id = j.at("id").get<int>();
  e.from = j.at("from").get<std::string>();
  e.msg = j.at("msg").get<std::string>();
  e.to = j.at("to").get<menu_code_type>();
}

}

