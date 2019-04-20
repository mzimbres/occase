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

// Contains the implementation of many test clients.

namespace rt::cli
{

template <class Mgr>
class session_shell;

// Does no proceed with the websocket handshake after stablishing the
// tcp connection. The server should drop such clients.
struct only_tcp_conn {
   using client_type = session_shell<only_tcp_conn>;

   struct options_type {
      login user;
   };

   only_tcp_conn(options_type) noexcept { }
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
   struct options_type {
      login user;
   };

   no_login(options_type) noexcept { }
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

/* This test client will hang on the channels and send the publisher a
 * user_msg. It will exit after receiving a pre-calculated number of
 * messages.
 */

class replier {
public:
   struct options_type {
      login user;
      int n_publishers;
   };

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

class publisher {
public:
   struct options_type {
      login user;
      int n_repliers;
   };

private:
   using client_type = session_shell<publisher>;
   using code_type = std::vector<std::vector<std::vector<int>>>;

   options_type op;

   bool server_echo = false;
   int post_id = -1;
   int user_msg_counter;

   std::stack<code_type> pub_stack;

   int send_post(std::shared_ptr<client_type> s) const;

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

// This class will be used to publish some messages to the server and
// exit quickly.
class publisher2 {
public:
   struct options_type {
      login user;
   };

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

// This class will be used to log in the server and wait for messages
// the user may have received while he was offline. It will wait for
// *expected_user_msgs* chat messages or one publish_ack.
class msg_pull {
public:
   struct options_type {
      login user;
      int expected_user_msgs;
   };

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

// This class will be used to register some users in the server.
class register1 {
public:
   struct options_type {
      login user;
   };

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

// This class is used to test if the server rejects logins with wrong
// credentials.
class login_err {
public:
   using options_type = register1::options_type;
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

// Closes the tcp connection right after writing it on the socket. The
// server should not be able to send the post_ack back to the app and
// send it to the database when the worker_session dies.
class early_close {
public:
   struct options_type {
      login user;
   };

private:
   using client_type = session_shell<early_close>;
   using code_type = std::vector<std::vector<std::vector<int>>>;

   options_type op;

   void send_post(std::shared_ptr<client_type> s) const;

public:
   early_close(options_type op_)
   : op {op_}
   { }

   int on_read(std::string msg, std::shared_ptr<client_type> s);
   int on_closed(boost::system::error_code ec)
      { throw std::runtime_error("early_close::on_closed"); return -1; }
   int on_handshake(std::shared_ptr<client_type> s);
   int on_connect() const noexcept { return 1;}
   auto const& get_login() const noexcept {return op.user;}
};

// Register n users in the server and return the credentials.
std::vector<login> test_reg(session_shell_cfg const& cfg, int n);

}


