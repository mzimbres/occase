#pragma once

#include <stack>
#include <string>
#include <memory>
#include <utility>
#include <stdexcept>

#include "config.hpp"
#include "json_utils.hpp"

namespace rt
{

template <class Mgr>
class client_session;

struct cmgr_gmsg_check_op {
   std::string user;
   int n_publishers;
   int msgs_per_channel;
};

class client_mgr_gmsg_check {
public:
   using options_type = cmgr_gmsg_check_op;
private:
   using client_type = client_session<client_mgr_gmsg_check>;
   options_type op;
   int tot_msgs;

public:
   client_mgr_gmsg_check(options_type op);
   int on_read(std::string msg, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec);
   int on_handshake(std::shared_ptr<client_type> s);
   int on_connect() const noexcept { return 1;}
   auto get_user() const {return op.user;}
};

}

