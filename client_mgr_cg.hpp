#pragma once

#include <stack>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>

#include "config.hpp"
#include "json_utils.hpp"
#include "menu_parser.hpp"

template <class Mgr>
class client_session;

// Creates groups.

struct cmgr_cg_op {
   std::string user;
   std::string expected;
};

class client_mgr_cg {
public:
   using options_type = cmgr_cg_op;
private:
   using client_type = client_session<client_mgr_cg>;
   options_type op;
   std::stack<std::string> cmds;

public:
   client_mgr_cg(options_type op_);
   ~client_mgr_cg();

   int on_read(std::string msg, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec);
   int on_handshake(std::shared_ptr<client_type> s);
   int on_connect() const noexcept { return 1;}
   auto get_user() const {return op.user;}
};

