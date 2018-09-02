#include "client_mgr_accept_timer.hpp"

#include "client_session.hpp"

int client_mgr_accept_timer::on_read(json j, std::shared_ptr<client_type> s)
{
   std::cerr << "Error: client_mgr_accept_timer::on_read." << std::endl;
   return -1;
}

int client_mgr_accept_timer::on_closed(boost::system::error_code ec)
{
   if (number_of_reconnects > 0) {
      //std::cerr << "on_closed: continue." << std::endl;
      return 1;
   }

   // We are done with outŕ test and we can inform the session it can
   // be closed.
   std::cout << "Test accept timer: ok" << std::endl;
   return -1;
}

int client_mgr_accept_timer::on_write(std::shared_ptr<client_type> s)
{
   std::cerr << "Error: client_mgr_accept_timer::on_write." << std::endl;
   return 1;
}

int client_mgr_accept_timer::on_handshake(std::shared_ptr<client_type> s)
{
   if (number_of_reconnects-- > 0)
      return 1;

   return -1;
}

