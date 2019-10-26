#pragma once

#include <iostream>

#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/bind_executor.hpp>

#include <boost/beast/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/rfc6455.hpp>

namespace net = boost::asio;
namespace ip = net::ip;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace ssl = boost::asio::ssl;
using tcp = net::ip::tcp;

namespace rt
{

inline
void fail(boost::system::error_code const& ec, char const* what)
{
   std::cerr << what << ": " << ec.message() << "\n";
}

/* Can be used to get the filename or the file extension.
*
*    s = "/" for file name.
*    s = "." for file extension.
*
* For example foo.bar will result in "foo" and "bar".
*/
inline
std::pair<boost::beast::string_view, boost::beast::string_view>
split(boost::beast::string_view path, char const s)
{
   if (std::empty(path))
      return {};

   if (path.back() == s)
      path = path.substr(0, std::size(path) - 1);

   auto const pos = path.rfind(s);
   if (pos == std::string::npos)
      return {};

   return {path.substr(0, pos), path.substr(pos + 1)};
}

}

