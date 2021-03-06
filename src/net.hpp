#pragma once

#include <iostream>

#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/deadline_timer.hpp>

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

namespace occase
{

using tcp_stream = beast::tcp_stream;
using ssl_stream = beast::ssl_stream<tcp_stream>;

bool load_ssl( ssl::context& ctx
             , std::string const& ssl_cert_file
             , std::string const& ssl_priv_key_file
             , std::string const& ssl_dh_file);

}

