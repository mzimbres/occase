#pragma once

#include <random>
#include <string>
#include <iterator>

#include <fmt/format.h>

namespace rt
{

template <class C>
auto ssize(C const& c)
{
   auto const size = std::size(c);
   return static_cast<int>(size);
}

void set_fd_limits(int fds);

// Creates and removes the pid file using RAII.
class pidfile_mgr {
private:
   std::string pidfile_;

public:
   pidfile_mgr(std::string const& pidfile);
   ~pidfile_mgr();
};

void daemonize();

class pwd_gen {
private:
   std::mt19937 gen;
   std::uniform_int_distribution<int> dist;

public:
   pwd_gen();

   std::string operator()(int size);
};

void drop_root_priviledges();

}

