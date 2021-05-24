#include "post.hpp"

#include <nlohmann/json.hpp>

namespace occase
{

void to_json(json& j, post const& e)
{
   j = json{ {"date", e.date.count()}
           , {"id", e.id}
           , {"visualizations", e.visualizations}
           , {"from", e.from}
           , {"nick", e.nick}
           , {"avatar", e.avatar}
           , {"description", e.description}
           , {"location", e.location}
           , {"product", e.product}
           , {"ex_details", e.ex_details}
           , {"in_details", e.in_details}
           , {"range_values", e.range_values}
           , {"images", e.images}
           };
}

void from_json(json const& j, post& e)
{
  e.date = date_type {j.at("date").get<long int>()};
  e.id = j.at("id").get<std::string>();
  e.visualizations = get_optional_field<int>(j, "visualizations");
  e.from = j.at("from").get<std::string>();
  e.nick = j.at("nick").get<std::string>();
  e.avatar = j.at("avatar").get<std::string>();
  e.description = j.at("description").get<std::string>();
  e.location = j.at("location").get<std::vector<int>>();
  e.product = j.at("product").get<std::vector<int>>();
  e.ex_details = j.at("ex_details").get<std::vector<int>>();
  e.in_details = j.at("in_details").get<std::vector<code_type>>();
  e.range_values = j.at("range_values").get<std::vector<int>>();
  e.images = j.at("images").get<std::vector<std::string>>();
}

std::string make_dir(std::string const& filename)
{
   assert(std::size(filename) >= sz::mms_filename_size);

   std::string path = "/";
   path.append(filename.data(), 0, sz::a);

   path.push_back('/');
   path.append(filename.data(), 0 + sz::a, sz::b);

   path.push_back('/');
   path.append(filename.data(), 0 + sz::a + sz::b, sz::c);

   return path;
}

} // occase

