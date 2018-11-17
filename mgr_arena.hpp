#pragma once

#include <thread>

#include <boost/asio.hpp>

#include "server_mgr.hpp"

namespace rt
{

class mgr_arena {
private:
   boost::asio::io_context ioc {1};
   boost::asio::signal_set signals;
   server_mgr mgr;
   std::thread thread;

   void run();

public:
   mgr_arena(server_mgr_cf const& cf);
   void join() { thread.join(); }
   auto& get_io_context() {return ioc;}
   auto& get_mgr() {return mgr;}
};

}

