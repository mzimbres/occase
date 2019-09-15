#include "json_utils.hpp"

#include <nlohmann/json.hpp>

namespace rt
{

void to_json(json& j, menu_elem const& e)
{
  j = json{{"data", e.data}, {"depth", e.depth}, {"version", e.version}};
}

void from_json(json const& j, menu_elem& e)
{
  e.data = j.at("data").get<std::string>();
  e.depth = j.at("depth").get<unsigned>();
  e.version = j.at("version").get<int>();
}

void to_json(json& j, post const& e)
{
   // Maybe we should not include the filter field below. The does not
   // need it.

   j = json{ {"id", e.id}
           , {"from", e.from}
           , {"body", e.body}
           , {"to", e.to}
           , {"features", e.features}
           , {"date", e.date}
           , {"range_values", e.range_values}
           };
}

void from_json(json const& j, post& e)
{
  e.id = j.at("id").get<int>();
  e.from = j.at("from").get<std::string>();
  e.body = j.at("body").get<std::string>();
  e.to = j.at("to").get<menu_code_type>();
  e.features = j.at("features").get<std::uint64_t>();
  e.date = j.at("date").get<long int>();
  e.range_values = j.at("range_values").get<std::vector<int>>();
}

}

