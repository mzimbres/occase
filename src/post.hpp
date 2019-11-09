#pragma once

#include <array>
#include <vector>
#include <string>
#include <chrono>
#include <ostream>
#include <cstdint>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace rt
{

// The sizes used to generate multimedia paths.
namespace sz
{

constexpr std::size_t a = 5;
constexpr std::size_t b = 2;
constexpr std::size_t c = 2;
constexpr std::size_t mms_filename_min_size = a + b + c;

}

using code_type = std::uint64_t;
using date_type = std::chrono::seconds;

struct post {
   int id;
   std::string from;
   std::string body;
   code_type to;
   code_type filter;
   code_type features;
   date_type date {0};
   std::vector<int> range_values;
};

inline
auto operator<(post const& a, post const& b) noexcept
   { return a.id < b.id; }

void to_json(json& j, post const& e);
void from_json(json const& j, post& e);

}

