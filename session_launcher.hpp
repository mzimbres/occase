#pragma once

#include <memory>
#include <stdexcept>

#include <boost/asio.hpp>

struct launcher_op {
   int begin;
   int end;
   std::chrono::milliseconds interval;
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
   mgr_op_type mgr_op;
   client_session_cf ccf;
   launcher_op lop;
   boost::asio::steady_timer timer;
 
public:
   session_launcher( boost::asio::io_context& ioc_
                   , mgr_op_type mgr_op_
                   , client_session_cf ccf_
                   , launcher_op lop_)
   : ioc(ioc_)
   , mgr_op(mgr_op_)
   , ccf(ccf_)
   , lop(lop_)
   , timer(ioc)
   {}

   ~session_launcher()
   {
      // TODO: Implement this.
      //std::vector<std::shared_ptr<client_session<client_mgr_sim>>> sessions;
      //auto sent = 0;
      //auto recv = 0;
      //for (auto const& session : sessions) {
      //   sent += session->get_sent_msgs();
      //   recv += session->get_recv_msgs();
      //}

      //std::cout << "Sent:     " << sent << std::endl;
      //std::cout << "Received: " << recv << std::endl;
   }

   void run(boost::system::error_code ec)
   {
      if (ec)
         throw std::runtime_error("No error expected here.");

      if (lop.begin == lop.end) {
         if (!std::empty(lop.final_msg))
            std::cout << lop.final_msg << " "
                      << lop.end << std::endl;
         return;
      }

      mgr_op.user = to_str(lop.begin);

      std::make_shared<client_type>( ioc
                                   , ccf
                                   , mgr_op)->run();

      timer.expires_after(lop.interval);

      auto handler = [p = this->shared_from_this()](auto ec)
      { p->run(ec); };

      timer.async_wait(handler);
      ++lop.begin;
   }
};

