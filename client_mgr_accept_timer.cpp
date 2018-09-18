#include "client_mgr_accept_timer.hpp"

#include <iostream>

#include "client_session.hpp"

int client_mgr_accept_timer::on_read(json j, std::shared_ptr<client_type> s)
{
   // We should not receive any message from the server in this test.
   std::cerr << "Error: client_mgr_accept_timer::on_read." << std::endl;
   err = true;
   return -1;
}

int client_mgr_accept_timer::on_closed(boost::system::error_code ec)
{
   //std::cout << "Test accept timer: ok" << std::endl;
   return -1;
}

int client_mgr_accept_timer::on_write(std::shared_ptr<client_type> s)
{
   // We will not write any message and therefore this function should
   // not be called.
   std::cerr << "Error: client_mgr_accept_timer::on_write." << std::endl;
   err = true;
   return 1;
}

