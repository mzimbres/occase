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

namespace rt::cli
{

template <class Mgr>
class session_shell;

struct only_tcp_no_ws_cfg {
   login user;
};

// Does no proceed with the websocket handshake after stablishing the
// tcp connection. This is meant to test if the server times out the
// connection.
struct only_tcp_no_ws {
   using client_type = session_shell<only_tcp_no_ws>;
   using options_type = only_tcp_no_ws_cfg;

   only_tcp_no_ws(only_tcp_no_ws_cfg) noexcept { }
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

// Stablishes the websocket connection but does not proceed with a
// login or register command. The server should drop the connection.
class no_login {
private:
   using client_type = session_shell<no_login>;

public:
   using options_type = only_tcp_no_ws_cfg;
   no_login(only_tcp_no_ws_cfg) noexcept { }
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

struct replier_cfg {
   login user;
   int n_publishers;
};

class replier {
public:
   using options_type = replier_cfg;

private:
   using client_type = session_shell<replier>;

   options_type op;

   int to_receive_posts;
   std::vector<menu_elem> menus;

   void talk_to( std::string user, long long id
               , std::shared_ptr<client_type> s);

public:
   replier(options_type op_)
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

struct publisher_cfg {
   login user;
   int n_repliers;
};

class publisher {
public:
   using options_type = publisher_cfg;
private:
   using client_type = session_shell<publisher>;
   using code_type = std::vector<std::vector<std::vector<int>>>;

   options_type op;

   bool server_echo = false;
   int post_id = -1;
   int user_msg_counter;

   std::stack<code_type> pub_stack;

   int send_group_msg(std::shared_ptr<client_type> s) const;

   int handle_msg(std::shared_ptr<client_type> s);

public:
   publisher(options_type op_)
   : op {op_}
   , user_msg_counter {op.n_repliers}
   { }

   int on_read(std::string msg, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec);
   int on_handshake(std::shared_ptr<client_type> s);
   int on_connect() const noexcept { return 1;}
   auto const& get_login() const noexcept {return op.user;}
};

struct publisher2_cfg {
   login user;
};

// This class will be used to publish some messages to the server and
// exit quickly.
class publisher2 {
public:
   using options_type = publisher2_cfg;
private:
   using client_type = session_shell<publisher2>;
   using code_type = std::vector<std::vector<std::vector<int>>>;

   options_type op;
   int msg_counter;
   std::vector<int> post_ids;

   int pub( menu_code_type const& pub_code
          , std::shared_ptr<client_type> s) const;

public:
   publisher2(options_type op_)
   : op {op_}
   { }

   int on_read(std::string msg, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec);
   int on_handshake(std::shared_ptr<client_type> s);
   int on_connect() const noexcept { return 1;}
   auto const& get_login() const noexcept {return op.user;}
   auto get_post_ids() const {return post_ids;}
};

struct msg_pull_cfg {
   login user;
   int expected_user_msgs;
};

// This class will be used to publish some messages to the server and
// exit quickly.
class msg_pull {
public:
   using options_type = msg_pull_cfg;
private:
   using client_type = session_shell<msg_pull>;
   using code_type = std::vector<std::vector<std::vector<int>>>;

   options_type op;
   std::vector<int> post_ids;

public:
   msg_pull(options_type op_)
   : op {op_}
   { }

   int on_read(std::string msg, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec)
      { throw std::runtime_error("msg_pull::on_closed"); return -1; }
   int on_handshake(std::shared_ptr<client_type> s);
   int on_connect() const noexcept { return 1;}
   auto const& get_login() const noexcept {return op.user;}
   auto get_post_ids() const {return post_ids;}
};

struct register_cfg {
   login user;
};

// This class will be used to register some users.
class register1 {
public:
   using options_type = register_cfg;
private:
   using client_type = session_shell<register1>;
   using code_type = std::vector<std::vector<std::vector<int>>>;

   options_type op;
   std::string pwd;

public:
   register1(options_type op_)
   : op {op_}
   { }

   int on_read(std::string msg, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec)
      { throw std::runtime_error("register1::on_closed"); return -1; }
   int on_handshake(std::shared_ptr<client_type> s);
   int on_connect() const noexcept { return 1;}
   auto const& get_login() const noexcept {return op.user;}
};

using login_err_cfg = register_cfg;

// This class will be used to register some users.
class login_err {
public:
   using options_type = register_cfg;
private:
   using client_type = session_shell<login_err>;
   using code_type = std::vector<std::vector<std::vector<int>>>;

   options_type op;
   std::string pwd;

public:
   login_err(options_type op_)
   : op {op_}
   { }

   int on_read(std::string msg, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec)
      { return -1; }
   int on_handshake(std::shared_ptr<client_type> s);
   int on_connect() const noexcept { return 1;}
   auto const& get_login() const noexcept {return op.user;}
};

std::vector<login> test_reg(session_shell_cfg const& cfg, int n);

}


