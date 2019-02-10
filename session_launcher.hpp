#pragma once

#include <memory>
#include <stdexcept>
#include <functional>

#include <boost/asio.hpp>

/*
 * This class is used to launch client sessions with a definite
 * launch_interval. This way we can avoid launching them all at once
 * and killing the server, especially when we are dealing with
 * thousands of sessions.
 *
 * It is possible to set a callback that will be called when finished
 * launching the sessions. This is usefull to start launching a timer
 * or other sessions.
 *
 * The user ids will be generated on the fly beginning at begin and
 * ending at end - 1. See launcher_cf.
 */

namespace rt
{

struct launcher_cf {
   int begin;
   int end;
   std::chrono::milliseconds launch_interval;
   std::string final_msg;
};

template <class T>
class session_launcher : 
   public std::enable_shared_from_this<session_launcher<T>> {
public:
   using mgr_type = T;
   using mgr_op_type = typename mgr_type::options_type;
   using client_type = client_session<mgr_type>;

private:
   boost::asio::io_context& ioc;
   mgr_op_type mgr_cf;
   client_session_cf session_cf;
   launcher_cf lcf;
   boost::asio::steady_timer timer;
   std::function<void(void)> call = [](){};
 
public:
   session_launcher( boost::asio::io_context& ioc_
                   , mgr_op_type mgr_op_
                   , client_session_cf ccf_
                   , launcher_cf lop_)
   : ioc(ioc_)
   , mgr_cf(mgr_op_)
   , session_cf(ccf_)
   , lcf(lop_)
   , timer(ioc)
   {}

   void set_on_end(std::function<void(void)> c) { call = c; }

   ~session_launcher()
   {
      if (!std::empty(lcf.final_msg))
         std::cout << lcf.final_msg << std::endl;
   }

   void run(boost::system::error_code ec)
   {
      if (ec)
         throw std::runtime_error("No error expected here.");

      if (lcf.begin == lcf.end) {
         timer.expires_after(lcf.launch_interval);
         timer.async_wait([p = this->shared_from_this()](auto ec)
                          { p->call(); });
         return;
      }

      mgr_cf.user = to_str(lcf.begin);

      std::make_shared<client_type>( ioc
                                   , session_cf
                                   , mgr_cf)->run();

      timer.expires_after(lcf.launch_interval);

      auto handler = [p = this->shared_from_this()](auto ec)
      { p->run(ec); };

      timer.async_wait(handler);
      ++lcf.begin;
   }
};

}

