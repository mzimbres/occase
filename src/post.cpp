#include "post.hpp"

#include <nlohmann/json.hpp>

namespace occase
{

void to_json(json& j, post const& e)
{
   j = json{ {"id", e.id}
           , {"from", e.from}
           , {"body", e.body}
           , {"date", e.date.count()}
           , {"range_values", e.range_values}
           };
}

void from_json(json const& j, post& e)
{
  e.id = j.at("id").get<int>();
  e.from = j.at("from").get<std::string>();
  e.body = j.at("body").get<std::string>();
  e.date = date_type {j.at("date").get<long int>()};
  e.range_values = j.at("range_values").get<std::vector<int>>();
}

std::string make_rel_path(std::string const& filename)
{
   if (std::size(filename) < sz::mms_filename_size)
      return {};

   std::string path = "/";
   path.append(filename.data(), 0, sz::a);

   path.push_back('/');
   path.append( filename.data(), 0 + sz::a
              , sz::b);

   path.push_back('/');
   path.append( filename.data(), 0 + sz::a + sz::b
              , sz::c);

   return path;
}

}

