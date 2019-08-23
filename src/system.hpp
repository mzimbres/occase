#pragma once

#include <string>

namespace rt
{

void set_fd_limits(int fds);
void daemonize();
void drop_root_priviledges();

// Creates and removes the pid file using RAII.
class pidfile_mgr {
private:
   std::string pidfile_;

public:
   pidfile_mgr(std::string const& pidfile);
   ~pidfile_mgr();
};

}

