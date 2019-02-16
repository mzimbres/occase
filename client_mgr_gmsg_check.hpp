#pragma once

#include <map>
#include <string>
#include <memory>
#include <utility>
#include <stdexcept>

#include "menu.hpp"
#include "config.hpp"
#include "json_utils.hpp"

namespace rt
{

/*
 * This test client will hang on the channels silently and check
 * whether we are receiving all messages.  When the desired number of
 * message per channel is reached it will unsubscribe from the
 * channel. A message from a channel from which it has unsubscribed
 * will cause an error.
 */

template <class Mgr>
class client_session;

struct cmgr_gmsg_check_op {
   std::string user;
   int n_publishers;
   int msgs_per_channel_per_user;
};

class client_mgr_gmsg_check {
public:
   using options_type = cmgr_gmsg_check_op;

private:
   using client_type = client_session<client_mgr_gmsg_check>;
   struct helper {
      int counter;
      bool acked = false;
      bool sent = false;
   };

   options_type op;
   std::map<std::string, helper> counters;
   int tot_msgs;
   std::vector<menu_elem> menus;

   void speak_to_publisher( std::string user, long long id
                          , std::shared_ptr<client_type> s);

public:
   client_mgr_gmsg_check(options_type op_)
   : op(op_) { }

   int on_read(std::string msg, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec);
   int on_handshake(std::shared_ptr<client_type> s);
   int on_connect() const noexcept { return 1;}
   auto get_user() const {return op.user;}
};

}

