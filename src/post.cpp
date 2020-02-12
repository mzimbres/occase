#include "post.hpp"

#include <nlohmann/json.hpp>

namespace occase
{

void to_json(json& j, post const& e)
{
   // Maybe we should not include the filter field below. The does not
   // need it.

   j = json{ {"id", e.id}
           , {"from", e.from}
           , {"body", e.body}
           , {"to", e.to}
           , {"filter", e.filter}
           , {"features", e.features}
           , {"date", e.date.count()}
           , {"range_values", e.range_values}
           };
}

void from_json(json const& j, post& e)
{
  e.id = j.at("id").get<int>();
  e.from = j.at("from").get<std::string>();
  e.body = j.at("body").get<std::string>();
  e.to = j.at("to").get<code_type>();
  e.filter = j.at("filter").get<code_type>();
  e.features = j.at("features").get<code_type>();
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

