#pragma once

#include <vector>
#include <array>
#include <string>
#include <ostream>
#include <cstdint>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace rt
{

constexpr auto menu_size = 2;

struct menu_elem {
   std::string data;
   int depth = 0;
   int version = 0;
};

using menu_elems_array_type = std::array<menu_elem, menu_size>;

inline
auto is_menu_empty(menu_elems_array_type const& m)
{
   return std::empty(m.front().data)
       || std::empty(m.back().data)
       || m.front().depth == 0
       || m.back().depth == 0;
}

inline
void to_json(json& j, menu_elem const& e)
{
  j = json{{"data", e.data}, {"depth", e.depth}, {"version", e.version}};
}

inline
void from_json(json const& j, menu_elem& e)
{
  e.data = j.at("data").get<std::string>();
  e.depth = j.at("depth").get<unsigned>();
  e.version = j.at("version").get<int>();
}


using channel_code_type = std::vector<int>;
using menu_channel_elem_type = std::vector<channel_code_type>;
using menu_code_type = std::array<menu_channel_elem_type, menu_size>;

struct post {
   int id;
   std::string from;
   std::string msg;
   std::string nick;
   menu_code_type to;
   std::uint64_t filter;
   long int date = 0;
   std::vector<std::uint64_t> ex_details;
   std::vector<std::uint64_t> in_details;

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
           , {"date", e.date}
           , {"ex_details", e.ex_details}
           , {"in_details", e.in_details}
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
  e.date = j.at("date").get<long int>();
  e.ex_details = j.at("ex_details").get<std::vector<std::uint64_t>>();
  e.in_details = j.at("in_details").get<std::vector<std::uint64_t>>();
}

}

