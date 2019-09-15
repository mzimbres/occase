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

void to_json(json& j, menu_elem const& e);
void from_json(json const& j, menu_elem& e);

using channel_code_type = std::vector<int>;
using menu_channel_elem_type = std::vector<channel_code_type>;
using menu_code_type = std::array<menu_channel_elem_type, menu_size>;
using menu_code_type2 = std::array<std::vector<std::uint64_t>, menu_size>;

namespace idx
{
   // These indexes determine the menu index that will be used as
   // channel. The rule is to use as channel the one where the user
   // will perform less subscription. For example, the user is not
   // likely to subscribe to thpusends of locations, on the other hand
   // it may be interested on hundreds of car models, specially if the
   // menu is really fine grained. Invert the indexes 0 and depending
   // on that.
   constexpr auto a = 0;
   constexpr auto b = 1; // Used as channel
}

struct post {
   int id;
   std::string from;
   std::string body;
   menu_code_type to;
   std::uint64_t features;
   long int date = 0;
   std::vector<int> range_values;
};

inline
auto operator<(post const& a, post const& b) noexcept
   { return a.id < b.id; }

void to_json(json& j, post const& e);
void from_json(json const& j, post& e);

}

