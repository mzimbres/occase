#include "json_utils.hpp"

#include <string>

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
  e.depth = j.at("version").get<int>();
}

}

