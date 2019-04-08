#pragma once

#include <iostream>

#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/websocket.hpp>

namespace net = boost::asio;
namespace beast = boost::beast;

namespace rt
{

inline void fail(boost::system::error_code const& ec, char const* what)
{
   std::cerr << what << ": " << ec.message() << "\n";
}

}

