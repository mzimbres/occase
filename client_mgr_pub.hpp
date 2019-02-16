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

/* This class is meant to perform the following test
 *
 * 1. Log in with menu versions 0 to get the server send us the menus.
 * 2. Subscribe to all channels received in 1.
 * 3. Send a publish to one of the channels.
 * 4. Wait for the publish ack.
 * 5. Wait the for the publish to be forwarded back.
 * 6. Wait for a user_msg/unread/read relative to publish that has
 *    been sent.
 * 7. Back to step 3. until a publish has been sent to all channels.
 */

namespace rt
{

template <class Mgr>
class client_session;

struct cmgr_sim_op {
   std::string user;
};

class client_mgr_pub {
public:
   using options_type = cmgr_sim_op;
private:
   using client_type = client_session<client_mgr_pub>;

   struct ch_msg_helper {
      bool server_echo = false;
      long long post_id = -1;
      std::vector<std::vector<std::vector<int>>> pub_code;
   };

   options_type op;
   std::stack<ch_msg_helper> pub_stack;

   int send_group_msg(std::shared_ptr<client_type> s) const;

public:
   client_mgr_pub(options_type op_)
   : op(op_) { }

   int on_read(std::string msg, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec);
   int on_handshake(std::shared_ptr<client_type> s);
   int on_connect() const noexcept { return 1;}
   auto get_user() const {return op.user;}
};

}

