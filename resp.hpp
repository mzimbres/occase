#pragma once

#include <string>

namespace aedis
{

bool is_valid(std::string const& str);
bool is_array(std::string const& str);

std::string_view get_simple_string(std::string const& str);

std::string gen_ping_cmd(std::string msg);

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

