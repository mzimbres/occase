#pragma once

#include <iostream>

#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/bind_executor.hpp>

namespace net = boost::asio;
namespace ip = net::ip;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;

namespace rt
{

inline
void fail(boost::system::error_code const& ec, char const* what)
{
   std::cerr << what << ": " << ec.message() << "\n";
}

}

