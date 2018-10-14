#pragma once

#include <string>

namespace aedis
{

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

