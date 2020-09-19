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

// The occase-db server generates names for multimedia files. In
// occase-mms folders will be built from these filenames in the
// following form
//
//    abcdefgh.jpg --> /a/b/cd/abcdefgh.jpg
//
// The total number of folders depends on the size of the character
// set that at the moment is 36.
// 
// To avoid collisions among filenames in the same directory we need
// to introduce the d below. Given the size of the character set when
// d = 4 we have 1679616 possible filenames in the same directory,
// which hugely exceeds the expected values, which much be around
// 1000.

constexpr std::size_t a = 1;
constexpr std::size_t b = 1;
constexpr std::size_t c = 2;
constexpr std::size_t d = 4;
constexpr std::size_t mms_filename_size = a + b + c + d;
constexpr std::size_t mms_dir_size = 3 + a + b + c;

}

/* This function receives as input the filename and returns the
 * relative path where it should be stored, which will be a string
 * in the form.
 *
 *    /x/y/zz
 *
 * The filename minimum length is expected to be mms_filename_size.
 * The length of the x, y and zz are as of sz::a, sz::b etc.
 *
 */
std::string make_rel_path(std::string const& filename);

using code_type = std::uint64_t;
using date_type = std::chrono::seconds;

struct post {
   date_type date {0};
   int on_search;
   int visualizations;
   int clicks;
   std::string id;
   std::string from;
   std::string nick;
   std::string avatar;
   std::string description;
   std::vector<int> location;
   std::vector<int> product;
   std::vector<int> ex_details;
   std::vector<code_type> in_details;
   std::vector<int> range_values;
   std::vector<std::string> images;
};

struct comp_post_date_less {
   auto operator()(post const& a, post const& b) const noexcept
      { return a.date < b.date; }
};

struct comp_post_id_less {
   auto operator()(post const& a, post const& b) const noexcept
      { return a.id < b.id; }
};

struct comp_post_id_equal {
   auto operator()(post const& a, post const& b) noexcept
      { return a.id == b.id; }
};

void to_json(json& j, post const& e);
void from_json(json const& j, post& e);

}

