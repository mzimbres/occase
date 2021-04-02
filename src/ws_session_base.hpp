#pragma once

#include "net.hpp"

#include <string>
#include <memory>

namespace occase {

enum class ev_res
{ register_ok
, register_fail
, login_ok
, login_fail
, subscribe_ok
, subscribe_fail
, publish_ok
, publish_fail
, chat_msg_ok
, chat_msg_fail
, presence_ok
, presence_fail
, unknown
};

struct ws_session_base {
   virtual void set_pub_hash(std::string hash) {};
   virtual std::string const& get_pub_hash() const noexcept = 0;
   virtual bool is_logged_in() const noexcept = 0;
   virtual void shutdown() = 0;
   virtual void send(std::string msg, bool persist) = 0;
   virtual void run(http::request<http::string_body, http::fields> req) = 0;
};

} // occase
