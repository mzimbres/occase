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
 * This test client will hang on the channels and send the publisher a
 * user_msg. It will exit after receiving a pre-calculated number of
 * messages.
 */

template <class Mgr>
class client_session;

struct cmgr_gmsg_check_op {
   std::string user;
   int n_publishers;
};

class client_mgr_gmsg_check {
public:
   using options_type = cmgr_gmsg_check_op;

private:
   using client_type = client_session<client_mgr_gmsg_check>;

   options_type op;

   int to_receive_posts;
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

