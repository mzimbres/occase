#pragma once

#include <vector>
#include <string>

namespace aedis
{

bool is_valid(std::string const& str);
bool is_array(std::string const& str);

std::string get_simple_string(std::string const& str);
std::string get_int(std::string const& str);
std::string get_bulky_string(std::string const& str);

std::string gen_bulky_string( std::string cmd
                            , std::vector<std::string> param);

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

