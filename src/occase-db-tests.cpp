#include <iostream>

#include <boost/asio.hpp>

#include "net.hpp"
#include "post.hpp"

using tcp_socket = net::use_awaitable_t<>::as_default_on_t<tcp::socket>;

namespace this_coro = net::this_coro;

void check_result(json const& j, char const* expected, char const* error)
{
   auto const res = j.at("result").get<std::string>();
   if (res != expected)
      throw std::runtime_error(error);
}

net::awaitable<void>
tcp_timeout(
   std::string const& host,
   std::string const& port)
{
   try {
      auto ex = co_await this_coro::executor;
      tcp::resolver resv(ex);
      auto const r = resv.resolve(host, port);
      tcp_socket socket {ex};
      co_await async_connect(socket, r);
      char data[1024];
      co_await socket.async_read_some(net::buffer(data));
   } catch (std::exception const& e) {
      //std::cerr << e.what() << std::endl;
   }
   std::cout << "Success: tcp_timeout." << std::endl;
}

auto
make_req(
   std::string const& host,
   std::string const& target,
   std::string const& body = {})
{
   http::request<http::string_body> req;
   req.version(11);
   req.method(http::verb::post);
   req.target(target);
   req.set(http::field::host, host);
   req.set(http::field::user_agent, "occase-db-test");
   req.set(http::field::content_type, "application/json");
   req.set(http::field::content_length, beast::to_static_string(std::size(body)));
   req.body() = body;
   return req;
}

auto make_search_body()
{
   json j = occase::post{};
   return j.dump();
}

net::awaitable<void>
posts_search(
   std::string const& host,
   std::string const& port,
   std::string const& target)
{
   std::string const body = make_search_body();

   auto ex = co_await this_coro::executor;
   tcp::resolver resolver(ex);
   tcp_socket stream(ex);
   auto const results = resolver.resolve(host, port);
   co_await async_connect(stream, results);

   auto const req = make_req(host, target, body);
   co_await http::async_write(stream, req);
   beast::flat_buffer b;
   http::response<http::string_body> res;
   co_await http::async_read(stream, b, res);

   //std::cout << res << std::endl;
   stream.shutdown(tcp::socket::shutdown_both);
   std::cout << "Success: search/count." << std::endl;
}

auto make_publish_body(std::string const from)
{
   occase::post p;
   //publish1='{"post":{"date":0, "id":"jsjsjs", "on_search":10, "visualizations":10, "click":20, "from":"marcelo", "nick":"","avatar":"","description":"","location":[1,2,3,4],"product":[1,2,3,4], "ex_details":[1,2,3], "in_details":[1,2,3], "range_values": [1,2,3], "images":[]}}'
   p.date = occase::date_type {0};
   p.on_search = 0;
   p.visualizations = 0;
   p.clicks = 0;
   p.id = "";
   p.from = from;
   p.nick = "";
   p.avatar = "";
   p.description = "";
   p.location = {1, 2, 3, 4};
   p.product = {1, 2, 3};
   p.ex_details = {1, 2, 3};
   p.in_details = {1, 2, 3};
   p.range_values = {1, 2, 3};
   p.images = {""};

   json j;
   j["user"] = "marcelo";
   j["key"] = "jTGcIm0rBMSP1BJRJNju3085zwgnVs9w";
   j["post"] = p;
   return j.dump();
}

/* This coroutine will
 *
 * 1. Get a user id.
 * 2. Publish a post with the id from 1.
 * 3. Reply to messages from users and leave.
 *
 */
net::awaitable<void>
publish(
   std::string const& host,
   std::string const& port)
{
   try {
      std::string post_id;
      std::string pub_id;
      std::string key;
      std::string user;
      std::string user_id;

      auto ex = co_await this_coro::executor;
      tcp::resolver resolver(ex);
      auto const results = resolver.resolve(host, port);

      {  // Gets a user id
	 tcp_socket pub_stream(ex);
	 co_await async_connect(pub_stream, results);
	 auto const req = make_req(host, "/get-user-id");
	 co_await http::async_write(pub_stream, req);
	 beast::flat_buffer b;
	 http::response<http::string_body> res;
	 co_await http::async_read(pub_stream, b, res);
	 pub_stream.shutdown(tcp::socket::shutdown_both);
	 std::cout << "Success: get-user-id " << res.body();
         auto const j = json::parse(res.body());
         key = j.at("key").get<std::string>();
         user = j.at("user").get<std::string>();
         user_id = j.at("user_id").get<std::string>();
      }

      {  // Publishes a post.
         tcp_socket pub_stream(ex);
         co_await async_connect(pub_stream, results);
         std::string const body = make_publish_body(user_id);
         auto const req = make_req(host, "/posts/publish", body);
         co_await http::async_write(pub_stream, req);
         beast::flat_buffer b;
         http::response<http::string_body> res;
         co_await http::async_read(pub_stream, b, res);
         pub_stream.shutdown(tcp::socket::shutdown_both);

         auto const j = json::parse(res.body());
         //auto const cmd = j.at("cmd").get<std::string>();
         //check_result(j, "ok", "publish");
         post_id = j.at("id").get<std::string>();
         std::cout << "Success: Publish " << res.body();
      }

      {
         websocket::stream<tcp_socket> ws {ex};
         co_await async_connect(beast::get_lowest_layer(ws), results);
         co_await ws.async_handshake(host + ":" + port, "/");

	 beast::multi_buffer read_buf;
	 {  // login
	    json j_msg;
	    j_msg["cmd"] = "login";
	    j_msg["user"] = user;
	    j_msg["key"] = key;

	    co_await ws.async_write(net::buffer(j_msg.dump()));
	    co_await ws.async_read(read_buf);
	    auto const foo = beast::buffers_to_string(read_buf.data());
	    read_buf.consume(std::size(read_buf));
	    std::cout << "Login: " << foo << std::endl;
	 }

	 {  // Receive and send messages.
	    json j_msg;
	    j_msg["cmd"] = "message";
	    j_msg["is_redirected"] = 0;
	    j_msg["type"] = "chat";
	    j_msg["refers_to"] = -1;
	    j_msg["to"] = user_id;
	    j_msg["message"] = "Tenho interesse.";
	    j_msg["post_id"] = post_id;
	    j_msg["nick"] = "Occase";
	    j_msg["id"] = 23;
	    for (;;) {
	       co_await ws.async_read(read_buf);
	       auto const foo = beast::buffers_to_string(read_buf.data());
	       read_buf.consume(std::size(read_buf));
	       std::cout << foo << std::endl;
	       co_await ws.async_write(net::buffer(j_msg.dump()));
	    }
	 }
	 co_await ws.async_close(beast::websocket::close_code::normal);
      }
   } catch (std::exception const& e) {
      std::cout << "Error: " << e.what() << std::endl;
   }
}

int main()
{
   std::string host {"127.0.0.1"};
   std::string port {"8080"};
   boost::asio::io_context ioc {1};
   net::co_spawn(ioc, tcp_timeout(host, port), net::detached);
   net::co_spawn(ioc, posts_search(host, port, "/posts/search"), net::detached);
   net::co_spawn(ioc, posts_search(host, port, "/posts/count"), net::detached);
   net::co_spawn(ioc, publish(host, port), net::detached);
   ioc.run();
}
