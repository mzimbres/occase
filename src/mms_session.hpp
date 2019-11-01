#pragma once

#include "net.hpp"

namespace rt
{

struct mms_session_cfg {
   std::string doc_root;
   std::string mms_key;
   std::uint64_t body_limit {1000000}; 

   // The maximum duration time for a http session.
   int http_session_timeout {30};
};

struct mms_worker {
   mms_session_cfg cfg;
   auto const& get_cfg() const noexcept {return cfg;}
};

class mms_session : public std::enable_shared_from_this<mms_session> {
public:
   using arg_type = mms_worker const&;

private:
   using file_body_type = http::file_body;
   using req_body_parser_type = http::request_parser<file_body_type>;
   using req_header_parser_type = http::request_parser<http::empty_body>;

   using resp_body_type = http::response<file_body_type>;

   beast::tcp_stream stream_;
   beast::flat_buffer buffer {8192};

   // We first read the header. This is needed on post to know in
   // advance the file name.
   req_header_parser_type header_parser;

   // We have to use a std::unique_ptr here while beast does not offer
   // correct move semantics for req_body_parser_type
   std::unique_ptr<req_body_parser_type> body_parser;

   http::response<http::dynamic_body> resp;

   // We we receive a get request we need a file_body response.
   std::unique_ptr<resp_body_type> file_body_resp;
   
   arg_type worker_;

   void post_handler();
   void get_handler();
   void on_read_post_body(boost::system::error_code ec, std::size_t n);
   void on_read_header(boost::system::error_code ec, std::size_t n);
   void write_response();

public:
   mms_session(tcp::socket socket, arg_type arg, ssl::context& ctx);
   void run(std::chrono::seconds s);
};

}
