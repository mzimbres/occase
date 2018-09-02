#include "client_mgr_accept_timer.hpp"

#include <iostream>

#include "client_session.hpp"

int client_mgr_accept_timer::on_read(json j, std::shared_ptr<client_type> s)
{
   // We should not receive any message from the server in this test.
   std::cerr << "Error: client_mgr_accept_timer::on_read." << std::endl;
   return -1;
}

int client_mgr_accept_timer::on_closed(boost::system::error_code ec)
{
   if (number_of_reconnects > 0) {
      //std::cerr << "on_closed: continue." << std::endl;
      // Continue ...
      return 1;
   }

   // We are done with the test and we can inform the session it can
   // be closed.
   std::cout << "Test accept timer: ok" << std::endl;
   return -1;
}

int client_mgr_accept_timer::on_write(std::shared_ptr<client_type> s)
{
   // We will not write any message a there fore this function should
   // not be called.
   std::cerr << "Error: client_mgr_accept_timer::on_write." << std::endl;
   return 1;
}

int client_mgr_accept_timer::on_handshake(std::shared_ptr<client_type> s)
{
   if (number_of_reconnects-- > 0)
      return 1;

   // We should not reach this point since when number_of_reconnects
   // closed sections are reached the on_close function above should
   // inform the session to stop reconnecting.
   return -1;
}

