#include "json_utils.hpp"

#include <string>

namespace rt
{

void to_json(json& j, group_info const& g)
{
  j = json{{"header", g.header}, {"hash", g.hash}};
}

void from_json(json const& j, group_info& u)
{
  u.header = j.at("header").get<std::vector<std::string>>();
  u.hash = j.at("hash").get<std::string>();
}

}

