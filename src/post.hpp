#pragma once

#include <array>
#include <vector>
#include <string>
#include <chrono>
#include <ostream>
#include <cstdint>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace occase
{

// The sizes used to generate multimedia paths.
namespace sz
{

// The occase-db server generates filenames for multimedia files. In
// occase-mms folders will be built from these filenames in the
// following form
//
//    abcdefgh.jpg --> /a/b/cd/abcdefgh.jpg
//
// The total number of folders with depend on the size of the
// character set that at the moment is 36.
// 
// To avoid collisions among filenames in the same directory we need
// the d below. Given the size of the character set when d = 4 we
// have 1679616 possible filenames in the same directory, which hugely
// exceeds the expected values, which much be around 1000.

constexpr std::size_t a = 1;
constexpr std::size_t b = 1;
constexpr std::size_t c = 2;
constexpr std::size_t d = 4;
constexpr std::size_t mms_filename_size = a + b + c + d;

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

inline
auto operator==(post const& a, post const& b) noexcept
   { return a.id == b.id; }

inline
auto operator!=(post const& a, post const& b) noexcept
   { return !operator==(a, b); }

void to_json(json& j, post const& e);
void from_json(json const& j, post& e);

}

