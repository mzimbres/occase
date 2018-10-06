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

struct cmgr_sim_op {
   std::string user;
   std::string expected;
   int number_of_groups;
};

class client_mgr_sim {
public:
   using options_type = cmgr_sim_op;
private:
   using client_type = client_session<client_mgr_sim>;
   options_type op;
   std::stack<std::string> cmds;
   std::stack<std::string> hashes;

   void send_group_msg(std::shared_ptr<client_type> s);

public:
   client_mgr_sim(options_type op);
   int on_read(std::string msg, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec);
   int on_handshake(std::shared_ptr<client_type> s);
   int on_connect() const noexcept { return 1;}
   auto get_user() const {return op.user;}
};

