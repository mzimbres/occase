#pragma once

#include <memory>
#include <stdexcept>
#include <functional>

#include <boost/asio.hpp>

#include "utils.hpp"
#include "client_session.hpp"

/*
 * This class is used to launch sessions with a definite
 * launch_interval. This way we can avoid launching them all at once
 * and killing the server, especially when we are dealing with
 * thousands of sessions.
 *
 * It is possible to set a callback that will be called when finished
 * launching the sessions. This is usefull to start launching a timer
 * or other sessions.
 */

namespace occase::cli
{

struct launcher_cfg {
   std::vector<login> logins;
   std::chrono::milliseconds launch_interval;
   std::string final_msg;
};

template <class T>
class session_launcher : 
   public std::enable_shared_from_this<session_launcher<T>> {
public:
   using mgr_type = T;
   using mgr_cfg_type = typename mgr_type::options_type;
   using client_type = session_shell<mgr_type>;

private:
   boost::asio::io_context& ioc;
   mgr_cfg_type mgr_cfg;
   session_shell_cfg session_cf;
   launcher_cfg cfg;
   boost::asio::steady_timer timer;
   std::function<void(void)> call = [](){};
   std::vector<std::shared_ptr<client_type>> sessions;
   int next_login = 0;
 
public:
   session_launcher( boost::asio::io_context& ioc_
                   , mgr_cfg_type mgr_op_
                   , session_shell_cfg ccf_
                   , launcher_cfg lop_)
   : ioc(ioc_)
   , mgr_cfg(mgr_op_)
   , session_cf(ccf_)
   , cfg(lop_)
   , timer(ioc)
   {}

   void set_on_end(std::function<void(void)> c) { call = c; }

   ~session_launcher()
   {
      if (!std::empty(cfg.final_msg))
         std::cout << "Finished launching: " << cfg.final_msg << std::endl;
   }

   void run(boost::system::error_code ec)
   {
      if (ec)
         throw std::runtime_error("No error expected here.");

      if (next_login == ssize(cfg.logins)) {
         timer.expires_after(cfg.launch_interval);
         timer.async_wait([p = this->shared_from_this()](auto ec)
                          { p->call(); });
         return;
      }

      mgr_cfg.cred = cfg.logins.at(next_login);

      auto session =
         std::make_shared<client_type>(ioc, session_cf, mgr_cfg);

      sessions.push_back(session);
      session->run();

      timer.expires_after(cfg.launch_interval);

      auto handler = [p = this->shared_from_this()](auto ec)
      { p->run(ec); };

      timer.async_wait(handler);
      ++next_login;
   }

   std::vector<std::shared_ptr<client_type>> get_sessions() const
      { return sessions; }
};

}

