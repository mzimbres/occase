#include "server_session.hpp"

namespace
{

void fail(boost::system::error_code ec, char const* what)
{
   std::cerr << what << ": " << ec.message() << "\n";
}

}

server_session::server_session( tcp::socket socket
                              , std::shared_ptr<server_data> sd_)
: ws(std::move(socket))
, strand(ws.get_executor())
, sd(sd_)
{ }

void server_session::run()
{
   auto handler = [p = shared_from_this()](auto ec)
   { p->on_accept(ec); };

   ws.async_accept(
      boost::asio::bind_executor(strand, handler));
}

void server_session::on_accept(boost::system::error_code ec)
{
   if (ec)
      return fail(ec, "accept");

   do_read();
}

void server_session::do_read()
{
   auto handler = [p = shared_from_this()](auto ec, auto n)
   { p->on_read(ec, n); };

   ws.async_read( buffer
                , boost::asio::bind_executor(
                     strand,
                     handler));
}

void server_session::on_read( boost::system::error_code ec
                            , std::size_t bytes_transferred)
{
   boost::ignore_unused(bytes_transferred);

   // This indicates that the session was closed
   if (ec == websocket::error::closed)
      return;

   if (ec) {
      fail(ec, "read");
      return;
   }

   try {
      std::stringstream ss;
      ss << boost::beast::buffers(buffer.data());
      //auto str = ss.str();
      //std::cout << str << std::endl;
      json j;
      ss >> j;

      //std::cout << j << std::endl;
      auto cmd = j["cmd"].get<std::string>();
      if (cmd == "login") {
         login_handler(std::move(j));
      } else if (cmd == "create_group") {
         create_group_handler(std::move(j));
      } else if (cmd == "join_group") {
         join_group_handler(std::move(j));
      } else if (cmd == "send_group_msg") {
         send_group_msg_handler(std::move(j));
      } else {
         std::cerr << "Server: Unknown command " << cmd << std::endl;
      }
   } catch (...) {
      std::cerr << "Server: Invalid json." << std::endl;
   }

   do_read();
}

void server_session::send_group_msg_handler(json j)
{
   //auto from = j["from"].get<int>();
   auto to = j["to"].get<int>();
   auto msg = j["msg"].get<std::string>();

   json bc;
   bc["cmd"] = "message";
   bc["message"] = msg;

   // TODO: use return type.
   sd->send_group_msg(bc.dump(), to);
}

void server_session::create_group_handler(json j)
{
   auto from = j["from"].get<int>();
   auto group_idx = sd->create_group(from);

   json resp;
   if (group_idx == -1) {
      std::cout << "Cannot create group." << std::endl;
      resp["result"] = "fail";
   } else {
      resp["result"] = "ok";
   }

   resp["cmd"] = "create_group_ack";
   resp["group_idx"] = group_idx;

   write(resp.dump());
}

void server_session::join_group_handler(json j)
{
   auto from = j["from"].get<int>();
   auto group_idx = j["group_idx"].get<int>();

   json resp;
   if (!sd->join_group(from, group_idx)) {
      std::cout << "Cannot join group." << std::endl;
      resp["result"] = "fail";
   } else {
      resp["result"] = "ok";
   }

   resp["cmd"] = "join_group_ack";
   resp["result"] = "ok";

   write(resp.dump());
}

void server_session::login_handler(json j)
{
   auto name = j["name"].get<std::string>();
   auto tel = j["tel"].get<std::string>();

   json resp;
   resp["cmd"] = "login_ack";
   resp["result"] = "ok";
   resp["user_idx"] = sd->add_user(tel, shared_from_this());

   std::cout << "New login from " << name << " " << tel
             << std::endl;

   write(resp.dump());
}

void server_session::write(std::string msg)
{
   ws.text(ws.got_text());

   auto handler = [p = shared_from_this()](auto ec, auto n)
   { p->on_write(ec, n); };

   ws.async_write( boost::asio::buffer(std::move(msg))
                 , boost::asio::bind_executor(
                      strand,
                      handler));
}

void server_session::on_write( boost::system::error_code ec
                             , std::size_t bytes_transferred)
{
   boost::ignore_unused(bytes_transferred);

   if (ec)
      return fail(ec, "write");

   // Clear the buffer
   buffer.consume(buffer.size());

}

