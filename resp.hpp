#pragma once

#include <vector>

namespace aedis
{

class resp_response {
private:
   std::vector<char> str;

public:
   resp_response(std::vector<char> resp)
   : str(std::move(resp))
   {}
   void process_response() const;
};

}

