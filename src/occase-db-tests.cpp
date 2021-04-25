#include <iostream>
#include <thread>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include "net.hpp"
#include "post.hpp"
#include "system.hpp"
#include "channel.hpp"

using tcp_socket = net::use_awaitable_t<>::as_default_on_t<tcp::socket>;

namespace this_coro = net::this_coro;

namespace occase
{

void assert_true(bool b, std::string const& msg = "assert_true")
{
   if (b)
     std::cout << "Success: " << msg << std::endl;
   else
     std::cout << "Error: " << msg << std::endl;
}

template <class T>
void assert_equal(T const& a, T const& b, std::string const& msg = "assert_equal")
{
   if (a == b)
     std::cout << "Success: " << msg << std::endl;
   else
     std::cout << "Error: " << msg << std::endl;
}

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
   req.set(http::field::content_length,
           beast::to_static_string(std::size(body)));
   req.body() = body;
   return req;
}

auto make_search_body()
{
   json j;
   j["post"] = occase::post{};
   return j.dump();
}

// Deprecated: Remove and use make_request.
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

struct user_cred {
   std::string user;
   std::string key;
   std::string user_id;
};

std::ostream& operator<<(std::ostream& os, user_cred const& c)
{
   os << "User: " << c.user << ", user id: " << c.user_id;
   return os;
}

auto make_publish_body(user_cred const& cred)
{
   occase::post p;
   //publish1='{"post":{"date":0, "id":"jsjsjs", "visualizations":10, "from":"marcelo", "nick":"","avatar":"","description":"","location":[1,2,3,4],"product":[1,2,3,4], "ex_details":[1,2,3], "in_details":[1,2,3], "range_values": [1,2,3], "images":[]}}'
   p.date = occase::date_type {0};
   p.visualizations = 0;
   p.id = "";
   p.from = cred.user_id;
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
   j["user"] = cred.user;
   j["key"] = cred.key;
   j["post"] = p;
   return j.dump();
}

void from_json(json const& j, user_cred& e)
{
  e.user = j.at("user").get<std::string>();
  e.key = j.at("key").get<std::string>();
  e.user_id = j.at("user_id").get<std::string>();
}

void to_json(json& j, user_cred const& e)
{
   j = json{ {"date", e.user}
           , {"key", e.key}
           , {"user_id", e.user_id}
           };
}

auto make_login(user_cred const& cred)
{
   json j;
   j["cmd"] = "login";
   j["user"] = cred.user;
   j["key"] = cred.key;

   return j.dump();
}

struct pub_ack {
   std::string admin_id;
   std::string id;
   std::size_t date;
};

std::ostream& operator<<(std::ostream& os, pub_ack const& c)
{
   os << "Id: " << c.id;
   return os;
}

void from_json(json const& j, pub_ack& e)
{
  e.admin_id = j.at("admin_id").get<std::string>();
  e.id = j.at("id").get<std::string>();
  e.date = j.at("date").get<std::size_t>();
}

void to_json(json& j, pub_ack const& e)
{
   j = json{ {"admin_id", e.admin_id}
           , {"id", e.id}
           , {"date", e.date}
           };
}

struct message {
   std::string cmd = "message";
   std::string to;
   std::string from;
   std::string message = "Tenho interesse";
   std::string post_id;
   std::string nick;
   std::string type = "chat";
   int is_redirected = 0;
   int refers_to = -1;
   int id = 23;
};

void log_message(user_cred const& cred, message const& msg)
{
   std::cout
      << "User " << cred.user_id
      << " received message to " << msg.to
      << " from " << msg.from
      << ". Type " << msg.type
      << std::endl;
}

std::ostream& operator<<(std::ostream& os, message const& c)
{
   os << "To: " << c.to << ", from " << c.from << ", post id: " << c.post_id;
   return os;
}

void from_json(json const& j, message& e)
{
  e.cmd = j.at("cmd").get<std::string>();
  e.post_id = j.at("post_id").get<std::string>();
  e.from = j.at("from").get<std::string>();

  e.to = occase::get_optional_field<std::string>(j, "to");
  e.message = occase::get_optional_field<std::string>(j, "message");
  e.nick = occase::get_optional_field<std::string>(j, "nick");
  e.type = occase::get_optional_field<std::string>(j, "type");
  e.is_redirected = occase::get_optional_field<int>(j, "is_redirected");
  e.refers_to = occase::get_optional_field<int>(j, "refers_to");
  e.id = occase::get_optional_field<int>(j, "id");
}

void to_json(json& j, message const& e)
{
   j = json{ {"cmd", e.cmd}
           , {"to", e.to}
           , {"message", e.message}
           , {"post_id", e.post_id}
           , {"nick", e.nick}
           , {"type", e.type}
           , {"from", e.from}
           , {"is_redirected", e.is_redirected}
           , {"refers_to", e.refers_to}
           , {"id", e.id}
           };
}

net::awaitable<http::response<http::string_body>>
make_request(
   tcp::resolver::results_type const& results,
   std::string const& target,
   std::string const& host,
   std::string const& body = {})
{
   auto ex = co_await this_coro::executor;
   tcp_socket stream(ex);
   co_await async_connect(stream, results);
   auto const req = make_req(host, target, body);
   co_await http::async_write(stream, req);
   beast::flat_buffer b;
   http::response<http::string_body> res;
   co_await http::async_read(stream, b, res);
   stream.shutdown(tcp::socket::shutdown_both);
   co_return res;
}

auto
make_message(
   std::string const& to,
   std::string const& post_id,
   std::string const& body = {})
{
   message msg;
   msg.to = to;
   msg.post_id = post_id;
   msg.message = body;

   json j = msg;
   return j.dump();
}

net::awaitable<void>
launch_replier(
   tcp::resolver::results_type const& results,
   std::string const& host,
   std::string const& port,
   std::string const& post_id,
   std::string const& user_id,
   int n_chat_msgs)
{
   try {
      auto ex = co_await this_coro::executor;

      auto const res1 =
         co_await net::co_spawn(
            ex,
            make_request(results, "/get-user-id", host),
            net::use_awaitable);

      assert_true(res1.result() == http::status::ok, "get-user-id");
      auto const j_cred = json::parse(res1.body());
      auto const cred = j_cred.get<user_cred>();

      websocket::stream<tcp_socket> ws {ex};
      co_await async_connect(beast::get_lowest_layer(ws), results);
      co_await ws.async_handshake(host + ":" + port, "/");

      beast::multi_buffer read_buf;
      {  // login
         co_await ws.async_write(net::buffer(make_login(cred)));
         co_await ws.async_read(read_buf);
         auto const foo = beast::buffers_to_string(read_buf.data());
         read_buf.consume(std::size(read_buf));
         //std::cout << "Login: " << foo << std::endl;
      }

      {  // Send and receive messages.
         auto const msg = make_message(user_id, post_id);
         for (auto i = 0; i < n_chat_msgs; ++i) {
            {  // Server ack
               co_await ws.async_write(net::buffer(msg));
               co_await ws.async_read(read_buf);
               auto const msg_str = beast::buffers_to_string(read_buf.data());
               read_buf.consume(std::size(read_buf));

               auto const j_msg = json::parse(msg_str);
               auto msg = j_msg.get<message>();
               log_message(cred, msg);

               assert_equal(cred.user_id, msg.to, "launch_replier");
               assert_equal(msg.type, {"server_ack"}, "launch_replier");
            }

            {  // App msg
               co_await ws.async_read(read_buf);
               auto const msg_str = beast::buffers_to_string(read_buf.data());
               read_buf.consume(std::size(read_buf));

               auto const j_msg = json::parse(msg_str);
               auto msg = j_msg.get<message>();
               log_message(cred, msg);

               assert_equal(cred.user_id, msg.to, "launch_replier");
               assert_equal(msg.type, {"chat"}, "launch_replier");
            }
         }
         std::cout << "Replier Leaving" << std::endl;
         co_await ws.async_close(beast::websocket::close_code::normal);
      }
   } catch (std::exception const& e) {
      std::cout << "Error: " << e.what() << std::endl;
   }
}

struct cred_pub_helper {
   user_cred cred;
   pub_ack pack;
};

/* Coroutine that gets a credential, publishes a post and returns
 * them.
 */
net::awaitable<cred_pub_helper>
cred_pub(
   tcp::resolver::results_type const& results,
   std::string const& host)
{
   try {
      auto ex = co_await this_coro::executor;
      auto const res1 =
         co_await net::co_spawn(
            ex,
            make_request(results, "/get-user-id", host),
            net::use_awaitable);

      assert_true(res1.result() == http::status::ok, "get-user-id");

      auto const cred = json::parse(res1.body()).get<user_cred>();

      auto const body = make_publish_body(cred);

      auto const res2 =
         co_await net::co_spawn(
            ex,
            make_request(results, "/posts/publish", host, body),
            net::use_awaitable);

      assert_true(res2.result() == http::status::ok, "get-user-id");

      auto const pack = json::parse(res2.body()).get<pub_ack>();

      co_return cred_pub_helper {cred, pack};

   } catch (std::exception const& e) {
      std::cout << "Error: " << e.what() << std::endl;
   }

   co_return cred_pub_helper {};
}

/* This coroutine will
 *
 * 1. Get a user id.
 * 2. Publish a post with the id from 1.
 * 3. Reply to messages from users and leave.
 *
 */
net::awaitable<void>
publisher(
   std::string const& host,
   std::string const& port,
   int n_chat_msgs)
{
   try {
      auto ex = co_await this_coro::executor;
      tcp::resolver resolver(ex);
      auto const results = resolver.resolve(host, port);

      auto const foo =
         co_await net::co_spawn(
            ex,
            cred_pub(results, host),
            net::use_awaitable);

      net::co_spawn(
         ex,
         launch_replier(results, host, port, foo.pack.id, foo.cred.user_id, n_chat_msgs),
         net::detached);

      {
         websocket::stream<tcp_socket> ws {ex};
         co_await async_connect(beast::get_lowest_layer(ws), results);
         co_await ws.async_handshake(host + ":" + port, "/");

         beast::multi_buffer read_buf;
         {  // login
            json j_msg;
            j_msg["cmd"] = "login";
            j_msg["user"] = foo.cred.user;
            j_msg["key"] = foo.cred.key;

            co_await ws.async_write(net::buffer(j_msg.dump()));
            co_await ws.async_read(read_buf);
            auto const foo = beast::buffers_to_string(read_buf.data());
            read_buf.consume(std::size(read_buf));
            std::cout << "Login: " << foo << std::endl;
         }

         {  
            for (auto i = 0; i < n_chat_msgs; ++i) {
               {  // Receive and send
                  co_await ws.async_read(read_buf);
                  auto const msg_str = beast::buffers_to_string(read_buf.data());
                  read_buf.consume(std::size(read_buf));

                  auto const j_msg = json::parse(msg_str);
                  auto msg = j_msg.get<message>();
                  log_message(foo.cred, msg);

                  assert_equal(foo.cred.user_id, msg.to, "publisher");
                  assert_equal(foo.pack.id, msg.post_id, "publisher");
                  assert_equal(msg.type, {"chat"}, "publisher");

                  auto const reply = make_message(msg.from, msg.post_id);
                  co_await ws.async_write(net::buffer(reply));
               }

               {  // Server ack.
                  co_await ws.async_read(read_buf);
                  auto const ack_str = beast::buffers_to_string(read_buf.data());
                  read_buf.consume(std::size(read_buf));
                  auto const j_msg = json::parse(ack_str);
                  auto msg = j_msg.get<message>();
                  log_message(foo.cred, msg);

                  assert_equal(foo.cred.user_id, msg.to, "publisher");
                  assert_equal(foo.pack.id, msg.post_id, "publisher");
                  assert_equal(msg.type, {"server_ack"}, "publisher");
               }
            }

            std::cout << "Publisher Leaving" << std::endl;
         }
         co_await ws.async_close(beast::websocket::close_code::normal);
      }
   } catch (std::exception const& e) {
      std::cout << "Error: " << e.what() << std::endl;
   }
}

/* Stablishes the websocket connection but doesn't proceed with a
 * login or register command. The server should drop the connection.
 */
net::awaitable<void>
no_login(
   std::string const& host,
   std::string const& port)
{
   try {
      auto ex = co_await this_coro::executor;
      tcp::resolver resolver(ex);
      auto const results = resolver.resolve(host, port);
      websocket::stream<tcp_socket> ws {ex};
      co_await async_connect(beast::get_lowest_layer(ws), results);
      co_await ws.async_handshake(host + ":" + port, "/");
      beast::multi_buffer read_buf;
      co_await ws.async_read(read_buf);
   } catch (std::exception const& e) {
      std::cout << "Success: No login " << e.what() << std::endl;
   }
}

net::awaitable<void>
offline(
   std::string const& host,
   std::string const& port)
{
   try {
      auto ex = co_await this_coro::executor;
      tcp::resolver resolver(ex);
      auto const results = resolver.resolve(host, port);

      // The credentials and the post of user 1.
      auto const u1 =
         co_await net::co_spawn(
            ex,
            cred_pub(results, host),
            net::use_awaitable);

      std::clog << "Post publisher: " << u1.cred << std::endl;
      
      {  // Creates a second user and sends a chat message to user 1.
         // Credentials for user 2.
         auto const res2 =
            co_await net::co_spawn(
               ex,
               make_request(results, "/get-user-id", host),
               net::use_awaitable);

	 assert_true(res2.result() == http::status::ok, "get-user-id");

	 auto const cred2 = json::parse(res2.body()).get<user_cred>();

         std::clog << "Peer: " << cred2 << std::endl;

         beast::multi_buffer read_buf;
         websocket::stream<tcp_socket> ws {ex};
         co_await async_connect(beast::get_lowest_layer(ws), results);
         co_await ws.async_handshake(host + ":" + port, "/");
         co_await ws.async_write(net::buffer(make_login(cred2)));
         co_await ws.async_read(read_buf);
         read_buf.consume(std::size(read_buf));
         co_await ws.async_write(net::buffer(make_message(u1.cred.user_id, u1.pack.id, "offline test")));
         co_await ws.async_read(read_buf);
         read_buf.consume(std::size(read_buf));
         co_await ws.async_close(beast::websocket::close_code::normal);
         std::clog << "Peer finsihed." << std::endl;
      }

      //std::this_thread::sleep_for(std::chrono::seconds {2});

      {  // Logs in user 1 and expects the message from user 2.
         std::clog << "Loggin to retrieve message: " << u1.cred << std::endl;
         beast::multi_buffer read_buf;
         websocket::stream<tcp_socket> ws {ex};
         co_await async_connect(beast::get_lowest_layer(ws), results);
         co_await ws.async_handshake(host + ":" + port, "/");
         co_await ws.async_write(net::buffer(make_login(u1.cred)));
         co_await ws.async_read(read_buf); // Consumes the login ack.
         read_buf.consume(std::size(read_buf));
         co_await ws.async_read(read_buf); // This should be the message.
         auto const foo = beast::buffers_to_string(read_buf.data());
         read_buf.consume(std::size(read_buf));
         std::cout << "Message received: " << foo << std::endl;
         co_await ws.async_close(beast::websocket::close_code::normal);
      }
   } catch (std::exception const& e) {
      std::cout << "Error: " << e.what() << std::endl;
   }
}

} // occase

namespace po = boost::program_options;

struct options {
   std::string host {"127.0.0.1"};
   std::string port {"8080"};
   int publishers = 10;
   int repliers = 10;
   int offline_tests = 10;
   int test = 2;
};

using namespace occase;

void channel_tests()
{
   {  // query location and product.
      post p1;
      p1.location = {1, 2, 3};
      p1.product = {1, 2, 3};

      post p2;
      p2.location = {1, 3, 3};
      p2.product = {1, 3, 3};

      channel chn;
      chn.add_post(p1);
      chn.add_post(p2);

      auto const r = chn.query(p1);
      assert_true(std::size(r) == 1u, "channel_tests");
      assert_equal(r.front().location, p1.location, "channel_test");
   }

   { // Visualizations
      post p1;
      p1.id = "1";
      p1.location = {1};
      p1.visualizations = 0;

      post p2;
      p2.id = "2";
      p2.location = {2};
      p2.visualizations = 0;

      post p3;
      p3.id = "3";
      p3.location = {3};
      p3.visualizations = 0;

      channel::visual_type v
      { {"1", 10}
      , {"3", 10}
      };

      channel chn;
      chn.add_post(p1);
      chn.add_post(p2);
      chn.add_post(p3);

      chn.load_visualizations(v);

      auto const r1 = chn.query(p1);
      assert_true(std::size(r1) == 1u, "channel_tests");
      assert_equal(r1.front().visualizations, 10, "channel_tests");

      auto const r2 = chn.query(p2);
      assert_true(std::size(r2) == 1u, "channel_tests");
      assert_equal(r2.front().visualizations, 0, "channel_tests");

      auto const r3 = chn.query(p3);
      assert_true(std::size(r3) == 1u, "channel_tests");
      assert_equal(r3.front().visualizations, 0, "channel_tests");
   }
}

int main(int argc, char* argv[])
{
   options op;
   po::options_description desc("Options");
   desc.add_options()
   ("help,h", "Produces the help message.")
   ("port,p", po::value<std::string>(&op.port)->default_value("8080") , "Server port.")
   ("ip,d" , po::value<std::string>(&op.host)->default_value("127.0.0.1"), "Server ip address.")
   ("publishers,u", po::value<int>(&op.publishers)->default_value(2), "Number of publishers.")
   ("repliers,c", po::value<int>(&op.repliers)->default_value(10), "Number of listeners.")
   ("offline-tests,l", po::value<int>(&op.offline_tests)->default_value(10), "Number of offline tests.")
   ( "test,r"
   , po::value<int>(&op.test)->default_value(1)
   , "The test to run:\n"
     "• 1: \ttcp_timeout.\n"
     "• 2: \tpost_search.\n"
     "• 3: \tpost count.\n"
     "• 4:  \tno_login.\n"
     "• 6:  \toffline messages.\n"
     "• 7:  \tunittests.\n"
   )
   ;

   po::variables_map vm;        
   po::store(po::parse_command_line(argc, argv, desc), vm);
   po::notify(vm);    

   if (vm.count("help")) {
      std::cout << desc << "\n";
      return 0;
   }

   set_fd_limits(100000);

   std::string target1 = "/posts/search";
   std::string target2 = "/posts/count";
   boost::asio::io_context ioc {1};

   if (op.test == 1)
      net::co_spawn(ioc, tcp_timeout(op.host, op.port), net::detached);

   if (op.test == 2)
      net::co_spawn(ioc, posts_search(op.host, op.port, target1), net::detached);

   if (op.test == 3)
      net::co_spawn(ioc, posts_search(op.host, op.port, target2), net::detached);

   if (op.test == 4)
      net::co_spawn(ioc, no_login(op.host, op.port), net::detached);

   if (op.test == 5) {
      for (auto i = 0; i < op.publishers; ++i)
	 net::co_spawn(ioc, publisher(op.host, op.port, op.repliers), net::detached);
   }

   if (op.test == 6) {
      for (auto i = 0; i < op.offline_tests; ++i)
	 net::co_spawn(ioc, offline(op.host, op.port), net::detached);
   }

   if (op.test == 7) {
      channel_tests();
   }

   ioc.run();
}
