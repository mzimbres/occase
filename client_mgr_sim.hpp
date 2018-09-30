#pragma once

#include <stack>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>

#include "config.hpp"
#include "json_utils.hpp"

template <class Mgr>
class client_session;

// Creates groups.

class client_mgr_sim {
private:
   using client_type = client_session<client_mgr_sim>;
   std::string user;
   std::string expected;
   std::stack<std::string> cmds;
   std::stack<std::string> hashes;

   void send_group_msg(std::shared_ptr<client_type> s);

public:
   client_mgr_sim( std::string exp
                 , std::vector<std::string> hashes_
                 , std::string user_);
   int on_read(json j, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec);
   int on_handshake(std::shared_ptr<client_type> s);
   int on_connect() const noexcept { return 1;}
};

