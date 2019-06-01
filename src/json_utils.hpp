#pragma once

#include <string>
#include <ostream>
#include <cstdint>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace rt
{

using menu_code_type = std::vector<std::vector<std::vector<int>>>;

struct post {
   int id;
   std::string from;
   std::string msg;
   std::string nick;
   menu_code_type to;
   std::uint64_t filter;

   friend
   auto operator<(post const& a, post const& b) noexcept
   {
      return a.id < b.id;
   }
};

inline
void to_json(json& j, post const& e)
{
   // Maybe we should not include the filter field below. The does not
   // need it.

   j = json{ {"id", e.id}
           , {"from", e.from}
           , {"msg", e.msg}
           , {"to", e.to}
           , {"filter", e.filter}
           , {"nick", e.nick}
           };
}

inline
void from_json(json const& j, post& e)
{
  e.id = j.at("id").get<int>();
  e.from = j.at("from").get<std::string>();
  e.msg = j.at("msg").get<std::string>();
  e.to = j.at("to").get<menu_code_type>();
  e.filter = j.at("filter").get<std::uint64_t>();
  e.nick = j.at("nick").get<std::string>();
}

}

