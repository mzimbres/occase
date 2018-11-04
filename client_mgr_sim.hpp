#pragma once

#include <set>
#include <stack>
#include <vector>
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

struct cmgr_sim_op {
   std::string user;
   std::string expected;
   int msgs_per_group;
};

class client_mgr_sim {
public:
   using options_type = cmgr_sim_op;
private:
   struct ch_msg_helper {
      bool ack = false;
      bool msg = false;
      std::string hash;
   };
   using client_type = client_session<client_mgr_sim>;
   options_type op;
   std::stack<std::string> cmds;
   std::vector<ch_msg_helper> hashes;
   std::size_t group_counter = 0;

   void send_group_msg(std::shared_ptr<client_type> s);

public:
   client_mgr_sim(options_type op);
   ~client_mgr_sim();
   int on_read(std::string msg, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec);
   int on_handshake(std::shared_ptr<client_type> s);
   int on_connect() const noexcept { return 1;}
   auto get_user() const {return op.user;}
};

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
   std::stack<std::string> cmds;
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

