#pragma once

#include "net.hpp"

namespace rt
{

struct img_session_cfg {
   std::string doc_root;
   std::string img_key;
};

class img_session : public std::enable_shared_from_this<img_session> {
public:
   using arg_type = img_session_cfg const&;

private:
   using file_body_type = http::file_body;
   using req_body_parser_type = http::request_parser<file_body_type>;
   using req_header_parser_type = http::request_parser<http::empty_body>;

   using resp_body_type = http::response<file_body_type>;

   tcp::socket socket;
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
   
   net::basic_waitable_timer<std::chrono::steady_clock> deadline;
   img_session_cfg const& cfg;

   void post_handler();
   void get_handler();
   void on_read_post_body(boost::system::error_code ec, std::size_t n);
   void on_read_header(boost::system::error_code ec, std::size_t n);
   void write_response();
   void check_deadline();
public:

   img_session(tcp::socket socket, arg_type arg);
   void accept();
};

}
