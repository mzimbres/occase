#pragma once

#include <vector>
#include <string>

namespace aedis
{

using const_iterator = std::string::const_iterator;

std::size_t get_length(char const*& p);

std::string_view get_simple_string(char const* begin);
std::string get_int(std::string const& str);
std::string_view get_bulky_string(char const* begin, std::size_t s);

std::string gen_resp_cmd(std::string cmd, std::vector<std::string> param);

class resp_response {
public:
   using container_type = std::string;
   using const_iterator = container_type::const_iterator;

private:
   container_type str;

public:
   resp_response(std::string resp)
   : str(std::move(resp))
   {}

   void process_response() const;
};

}

