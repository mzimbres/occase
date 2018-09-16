#include "json_utils.hpp"

#include <string>

std::ostream& operator<<(std::ostream& os, user_bind const& info)
{
   os << "\n"
      << "   Tel:   " << info.tel   << "\n"
      << "   Host:  " << info.host  << "\n"
      << "   Index: " << info.index << "\n";

   return os;
}

void to_json(json& j, user_bind const& u)
{
  j = json{{"tel", u.tel}, {"host", u.host}, {"index", u.index}};
}

void to_json(json& j, group_info const& g)
{
  j = json{{"header", g.header}, {"hash", g.hash}};
}

void from_json(json const& j, user_bind& u)
{
  u.tel = j.at("tel").get<std::string>();
  u.host = j.at("host").get<std::string>();
  u.index = j.at("index").get<int>();
}

void from_json(json const& j, group_info& u)
{
  u.header = j.at("header").get<std::vector<std::string>>();
  u.hash = j.at("hash").get<std::string>();
}

