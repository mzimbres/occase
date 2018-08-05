#pragma once

#include <string>

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/websocket.hpp>

#include <nlohmann/json.hpp>

// Internally users and groups will be refered to by their index on a
// vector.
using index_type = int;

// This is the type we will use to associate a user  with something
// that identifies them in the real world like their phone numbers or
// email.
using id_type = std::string;


using json = nlohmann::json;

using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;

