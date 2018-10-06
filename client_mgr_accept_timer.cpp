#include "client_mgr_accept_timer.hpp"

#include <iostream>

#include "client_session.hpp"

int cmgr_handshake_tm::on_read(std::string msg, std::shared_ptr<client_type> s) const 
{
   throw std::runtime_error("Error.");
   return 1;
}

int cmgr_handshake_tm::on_closed(boost::system::error_code ec) const 
{
   throw std::runtime_error("Error.");
   return 1;
}

int cmgr_handshake_tm::on_write(std::shared_ptr<client_type> s) const
{
   throw std::runtime_error("Error.");
   return 1;
}

int cmgr_handshake_tm::on_handshake(std::shared_ptr<client_type> s) const 
{
   throw std::runtime_error("Error.");
   return 1;
}

int cmgr_handshake_tm::on_connect() const
{
   return -1;
}

int client_mgr_accept_timer::on_read(std::string msg, std::shared_ptr<client_type> s)
{
   // We should not receive any message from the server in this test.
   throw std::runtime_error("client_mgr_accept_timer::on_read");
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
   throw std::runtime_error("client_mgr_accept_timer::on_write");
   return 1;
}

