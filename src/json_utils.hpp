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

inline
auto has_empty_products(menu_code_type const& e)
   { return std::empty(e.back()); }

struct post {
   int id;
   std::string from;
   std::string body;
   menu_code_type to;
   std::uint64_t filter;
   long int date = 0;
   std::vector<std::uint64_t> ex_details;
   std::vector<std::uint64_t> in_details;
   std::vector<int> range_values;
};

inline
auto operator<(post const& a, post const& b) noexcept
   { return a.id < b.id; }

void to_json(json& j, post const& e);
void from_json(json const& j, post& e);

}

