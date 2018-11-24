#pragma once

#include <thread>

#include <boost/asio.hpp>

#include "server_mgr.hpp"

namespace rt
{

class mgr_arena {
private:
   server_mgr mgr;
   std::thread thread;

public:
   mgr_arena(server_mgr_cf const& cf)
   : mgr {cf}
   , thread {[this]() { mgr.run(); }}
   { }
   void join() { thread.join(); }
   auto& get_mgr() {return mgr;}
};

}

