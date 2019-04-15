#pragma once

#include <map>
#include <set>
#include <stack>
#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <stdexcept>

#include "menu.hpp"
#include "config.hpp"
#include "json_utils.hpp"
#include "client_session.hpp"

namespace rt
{

template <class Mgr>
class client_session;

// Tests if the server drops a session that does not proceed with
// authentication.
// TODO: Add a timer here after which we should emit an error if the
// server does not drop the connection.

struct cmgr_handshake_op {
   login user;
};

struct cmgr_handshake_tm {
   using client_type = client_session<cmgr_handshake_tm>;
   using options_type = cmgr_handshake_op;
   cmgr_handshake_tm(cmgr_handshake_op) noexcept { }
   auto on_read(std::string msg, std::shared_ptr<client_type> s) const 
      { throw std::runtime_error("Error."); return 1; }
   auto on_closed(boost::system::error_code ec) const 
      { throw std::runtime_error("Error."); return 1; }
   auto on_write(std::shared_ptr<client_type> s) const
      { throw std::runtime_error("Error."); return 1; }
   auto on_handshake(std::shared_ptr<client_type> s) const 
      { throw std::runtime_error("Error."); return 1; }
   auto on_connect() const noexcept
      { return -1; }
   auto get_login() const noexcept
      {return login {};}
};

class client_mgr_accept_timer {
private:
   using client_type = client_session<client_mgr_accept_timer>;

public:
   using options_type = cmgr_handshake_op;
   client_mgr_accept_timer(cmgr_handshake_op) noexcept { }
   auto on_read(std::string msg, std::shared_ptr<client_type> s)
      { throw std::runtime_error("accept_timer::on_read"); return -1; }
   auto on_closed(boost::system::error_code ec) const noexcept
      { return -1; }
   auto on_write(std::shared_ptr<client_type> s)
      { throw std::runtime_error("accept_timer::on_write"); return 1; }
   auto on_handshake(std::shared_ptr<client_type> s)
      { return -1;}
   auto on_connect() const noexcept
      { return 1;}
   auto get_login() const noexcept
      {return login {};}
};

/*
 * This test client will hang on the channels and send the publisher a
 * user_msg. It will exit after receiving a pre-calculated number of
 * messages.
 */

struct cmgr_gmsg_check_op {
   login user;
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
   auto const& get_login() const noexcept {return op.user;}
};

/* This class is meant to perform the following test
 *
 * 1. Log in with menu versions 0 to get the server send us the menus.
 * 2. Subscribe to all channels received in 1.
 * 3. Send a publish to one of the channels.
 * 4. Wait for the publish ack.
 * 5. Wait the for the publish to be forwarded back.
 * 6. Wait for a user_msg directed to publish that has been sent,
 *    from each of the listener sessions.
 * 7. Back to step 3. until a publish has been sent to all channels.
 *
 * Notice: When more than one of this clients are started it may
 * appear on the server logs a failed write. This happens because
 * these clients will exit while the session on the server is still
 * alive and message to a channel are still sent. It is to investigate
 * however why they are lasting so long as we sent a clean wensocket
 * frame.
 */

struct cmgr_sim_op {
   login user;
   int n_listeners;
};

class client_mgr_pub {
public:
   using options_type = cmgr_sim_op;
private:
   using client_type = client_session<client_mgr_pub>;
   using code_type = std::vector<std::vector<std::vector<int>>>;

   options_type op;

   bool server_echo = false;
   int post_id = -1;
   int user_msg_counter;

   std::stack<code_type> pub_stack;

   int send_group_msg(std::shared_ptr<client_type> s) const;

   int handle_msg(std::shared_ptr<client_type> s);

public:
   client_mgr_pub(options_type op_)
   : op {op_}
   , user_msg_counter {op.n_listeners}
   { }

   int on_read(std::string msg, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec);
   int on_handshake(std::shared_ptr<client_type> s);
   int on_connect() const noexcept { return 1;}
   auto const& get_login() const noexcept {return op.user;}
};

struct test_pub_cfg {
   login user;
};

// This class will be used to publish some messages to the server and
// exit quickly.
class test_pub {
public:
   using options_type = test_pub_cfg;
private:
   using client_type = client_session<test_pub>;
   using code_type = std::vector<std::vector<std::vector<int>>>;

   options_type op;
   int msg_counter;
   std::vector<int> post_ids;

   int pub( menu_code_type const& pub_code
          , std::shared_ptr<client_type> s) const;

public:
   test_pub(options_type op_)
   : op {op_}
   { }

   int on_read(std::string msg, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec);
   int on_handshake(std::shared_ptr<client_type> s);
   int on_connect() const noexcept { return 1;}
   auto const& get_login() const noexcept {return op.user;}
   auto get_post_ids() const {return post_ids;}
};

struct test_msg_pull_cfg {
   login user;
   int expected_user_msgs;
};

// This class will be used to publish some messages to the server and
// exit quickly.
class test_msg_pull {
public:
   using options_type = test_msg_pull_cfg;
private:
   using client_type = client_session<test_msg_pull>;
   using code_type = std::vector<std::vector<std::vector<int>>>;

   options_type op;
   std::vector<int> post_ids;

public:
   test_msg_pull(options_type op_)
   : op {op_}
   { }

   int on_read(std::string msg, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec)
      { throw std::runtime_error("test_msg_pull::on_closed"); return -1; }
   int on_handshake(std::shared_ptr<client_type> s);
   int on_connect() const noexcept { return 1;}
   auto const& get_login() const noexcept {return op.user;}
   auto get_post_ids() const {return post_ids;}
};

struct test_register_cfg {
   login user;
};

// This class will be used to register some users.
class test_register {
public:
   using options_type = test_register_cfg;
private:
   using client_type = client_session<test_register>;
   using code_type = std::vector<std::vector<std::vector<int>>>;

   options_type op;
   std::string pwd;

public:
   test_register(options_type op_)
   : op {op_}
   { }

   int on_read(std::string msg, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec)
      { throw std::runtime_error("test_register::on_closed"); return -1; }
   int on_handshake(std::shared_ptr<client_type> s);
   int on_connect() const noexcept { return 1;}
   auto const& get_login() const noexcept {return op.user;}
};

std::vector<login> test_reg(client_session_cf const& cfg, int n);

}


