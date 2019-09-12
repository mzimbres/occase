#pragma once

#include <memory>

#include "worker.hpp"
#include "net.hpp"

namespace rt
{

class worker;

class http_session : public std::enable_shared_from_this<http_session> {
private:
   tcp::socket socket_;
   beast::flat_buffer buffer_{8192};
   http::request<http::dynamic_body> request_;
   http::response<http::dynamic_body> response_;
   net::steady_timer deadline_;
   worker const& worker_;
   worker_stats stats {};

   void read_request();
   void process_request();
   void create_response();
   void write_response();
   void check_deadline();

public:
   http_session(tcp::socket socket , worker const& w);
   void start();
};

}

