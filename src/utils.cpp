#include "utils.hpp"

namespace rt
{

std::pair<std::string, std::string> split(std::string data)
{
   auto const pos = data.find_first_of(':');
   if (pos == std::string::npos)
      return {};

   if (1 + pos == std::size(data))
      return {};

   return {data.substr(0, pos), data.substr(pos + 1)};
}

}

