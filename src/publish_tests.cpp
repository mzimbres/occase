#include <stack>
#include <chrono>
#include <thread>
#include <memory>
#include <vector>
#include <string>
#include <utility>
#include <sstream>
#include <cstdlib>
#include <iostream>

#include <boost/asio/signal_set.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include "system.hpp"
#include "config.hpp"
#include "menu.hpp"
#include "test_clients.hpp"
#include "client_session.hpp"
#include "session_launcher.hpp"

using namespace rt;
using namespace rt::cli;

namespace po = boost::program_options;

struct options {
   std::string host {"127.0.0.1"};
   std::string port {"8080"};
   int n_publishers = 10;
   int n_repliers = 10;
   int n_leave_after_n_posts = 10;
   int handshake_tm = 3;
   int launch_interval = 100;
   int auth_timeout = 3;
   int test = 2;

   auto make_session_cf() const
   {
      return session_shell_cfg
      { {host} 
      , {port}
      , std::chrono::seconds {handshake_tm}
      , std::chrono::seconds {auth_timeout}
      };
   }

   auto make_pub_cfg(std::vector<login> logins) const
   {
      return launcher_cfg
      { logins
      , std::chrono::milliseconds {launch_interval}
      , {}
      };
   }

   auto make_cfg(std::vector<login> logins) const
   {
      return launcher_cfg
      { logins
      , std::chrono::milliseconds {launch_interval}
      , {}
      };
   }

   auto make_launcher_empty_cfg(int n_logins) const
   {
      return launcher_cfg
      { std::vector<login>{static_cast<std::size_t>(n_logins)}
      , std::chrono::milliseconds {launch_interval}
      , {}
      };
   }
};

class timer {
private:
  std::chrono::time_point<std::chrono::system_clock> start_;
public:
  timer() { }
  void start()
  {
     start_ = std::chrono::system_clock::now();
  }
  auto get_count_now() const
  { 
    auto const end = std::chrono::system_clock::now();
    auto diff = end - start_;
    auto diff2 = std::chrono::duration_cast<std::chrono::seconds>(diff);
    return diff2.count();
  }
  ~timer() { }
};

void test_online(options const& op)
{
   boost::asio::io_context ioc;

   // Last we start the publishers.
   auto logins1 =
      test_reg( op.make_session_cf()
              , op.make_launcher_empty_cfg(op.n_publishers));

   auto next1 = [&logins1, &ioc, &op]()
   {
      using client_type = publisher;
      using config_type = client_type::options_type;

      auto const s = std::make_shared<session_launcher<publisher>
                      >( ioc
                       , config_type {{}, op.n_repliers}
                       , op.make_session_cf()
                       , op.make_pub_cfg(logins1)
                       );

      s->run({});
   };

   // Afterwards we start the repliers.
   auto const logins2 =
      test_reg( op.make_session_cf()
              , op.make_launcher_empty_cfg(op.n_repliers));

   auto next2 = [next1, &logins2, &ioc, &op]()
   {
      using client_type = replier;
      using config_type = client_type::options_type;

      auto const s = std::make_shared<session_launcher<client_type>
                      >( ioc
                       , config_type {{}, op.n_publishers}
                       , op.make_session_cf()
                       , op.make_cfg(logins2)
                       );
      
      // When finished launching the listeners we begin to launch the
      // publishers.
      s->set_on_end(next1);
      s->run({});
   };

   // And first we start the leave after n posts sessions.
   auto const logins3 =
      test_reg( op.make_session_cf()
              , op.make_launcher_empty_cfg(op.n_leave_after_n_posts));

   using client_type = leave_after_n_posts;
   using config_type = client_type::options_type;

   auto const s = std::make_shared<session_launcher<client_type>
                   >( ioc
                    , config_type {{}, op.n_publishers}
                    , op.make_session_cf()
                    , op.make_cfg(logins3)
                    );
   
   s->set_on_end(next2);
   s->run({});

   ioc.run();

   std::cout << "Test ok: Online messages." << std::endl;
}

void test_offline(options const& op)
{
   boost::asio::io_context ioc;

   using client_type1 = publisher2;
   using config_type1 = client_type1::options_type;
   using session_type1 = session_shell<client_type1>;

   auto const l1 =
      test_reg(op.make_session_cf(), op.make_launcher_empty_cfg(1));

   auto s1 = 
      std::make_shared<session_type1>( ioc
                                     , op.make_session_cf()
                                     , config_type1 {l1.front()});
   s1->run();
   ioc.run();
   auto post_ids = s1->get_mgr().get_post_ids();
   std::sort(std::begin(post_ids), std::end(post_ids));
   auto const n_post_ids = ssize(post_ids);

   using client_type2 = replier;
   using config_type2 = client_type2::options_type;
   using session_type2 = session_shell<replier>;

   auto const l2 = test_reg( op.make_session_cf()
                           , op.make_launcher_empty_cfg(1));
   std::make_shared<session_type2>( ioc
                                  , op.make_session_cf()
                                  , config_type2 {l2.front(), 1}
                                  )->run();
   ioc.restart();
   ioc.run();

   using client_type3 = msg_pull;
   using config_type3 = client_type3::options_type;
   using session_type3 = session_shell<client_type3>;

   auto s3 = 
      std::make_shared<session_type3>( ioc
                                     , op.make_session_cf()
                                     , config_type3
                                       {l1.front(), n_post_ids});
   s3->run();
   ioc.restart();
   ioc.run();

   auto post_ids2 = s3->get_mgr().get_post_ids();
   std::sort(std::begin(post_ids2), std::end(post_ids2));

   if (post_ids2 == post_ids)
      std::cout << "Test ok: Offline messages." << std::endl;
   else
      std::cout << "Test fail: Offline messages." << std::endl;
}

void read_only_tests(options const& op)
{
   using client_type1 = only_tcp_conn;
   using config_type1 = only_tcp_conn::options_type;

   boost::asio::io_context ioc {1};

   std::make_shared< session_launcher<client_type1>
                   >( ioc
                    , config_type1 {}
                    , op.make_session_cf()
                    , op.make_launcher_empty_cfg(300)
                    )->run({});

   using client_type2 = no_login;
   using config_type2 = no_login::options_type;

   std::make_shared< session_launcher<client_type2>
                   >( ioc
                    , config_type2 {}
                    , op.make_session_cf()
                    , op.make_launcher_empty_cfg(300)
                    )->run({});

   ioc.run();

   std::cout << "Test ok: Only tcp, no ws handshake (timeout test)."
             << std::endl;

   std::cout << "Test ok: No register/login (timeout test)."
             << std::endl;
}

void test_login_error(options const& op)
{
   using client_type = login_err;
   using config_type = client_type::options_type;
   using session_type = session_shell<client_type>;

   {
      // First test: Here we request a user_id from the server and
      // sets a wrong password to see whether the server refuses to
      // login the user.
      auto l1 = test_reg( op.make_session_cf()
                        , op.make_launcher_empty_cfg(op.n_publishers));

      l1.front().pwd = "Kabuf";

      boost::asio::io_context ioc;
      auto s1 = 
         std::make_shared<session_type>( ioc, op.make_session_cf()
                                       , config_type {l1.front()});
      s1->run();
      ioc.run();
      std::cout << "Test ok: Correct user id, wrong pwd." << std::endl;
   }

   {
      // Second test: Here we request a user_id from the server and
      // sets a wrong password to see whether the server refuses to
      // login the user.
      login invalid {"Kabuf", "Magralha"};

      boost::asio::io_context ioc;
      auto s1 = 
         std::make_shared<session_type>( ioc, op.make_session_cf()
                                       , config_type {invalid});
      s1->run();
      ioc.run();
      std::cout << "Test ok: Invalid user id." << std::endl;
   }
}

void test_early_close(options const& op)
{
   boost::asio::io_context ioc {1};

   auto const l1 = test_reg( op.make_session_cf()
                           , op.make_launcher_empty_cfg(1));

   {
      using client_type = early_close;
      using config_type = client_type::options_type;
      using session_type = session_shell<client_type>;

      auto s = 
         std::make_shared<session_type>( ioc, op.make_session_cf()
                                       , config_type {l1.front()});
      s->run();
      ioc.run();
   }

   // This value should be kept in sync with the pong timeout in
   // test.conf. The reason is that we have to wait untill the
   // websocket session in the server dies and the messages that could
   // not be sent back to the client are sent to the db. Only after
   // that we want to log in and retrieve the messages.
   std::this_thread::sleep_for(std::chrono::seconds {10});

   {
      using client_type = msg_pull;
      using config_type = client_type::options_type;
      using session_type = session_shell<client_type>;

      auto s = 
         std::make_shared<session_type>( ioc
                                       , op.make_session_cf()
                                       , config_type {l1.front(), 0});
      s->run();
      ioc.restart();
      ioc.run();
   }

   std::cout << "Test ok: Early close after post." << std::endl;
}

int main(int argc, char* argv[])
{
   try {
      options op;
      po::options_description desc("Options");
      desc.add_options()
         ("help,h", "Produces the help message.")

         ( "port,p"
         , po::value<std::string>(&op.port)->default_value("8080")
         , "Server port.")

         ("ip,d"
         , po::value<std::string>(&op.host)->default_value("127.0.0.1")
         , "Server ip address.")

         ("publishers,u"
         , po::value<int>(&op.n_publishers)->default_value(2)
         , "Number of publishers.")

         ("listeners,c"
         , po::value<int>(&op.n_repliers)->default_value(10)
         , "Number of listeners.")

         ("post-listeners,y"
         , po::value<int>(&op.n_leave_after_n_posts)->default_value(10)
         , "Number of listeners that will leave after receiving n posts.")

         ("launch-interval,g"
         , po::value<int>(&op.launch_interval)->default_value(100)
         , "Interval used to launch test clients.")

         ("handshake-timeout,k"
         , po::value<int>(&op.handshake_tm)->default_value(3)
         , "Time before which the server should have given "
           "up on the handshake in seconds.")

         ("auth-timeout,l"
         , po::value<int>(&op.auth_timeout)->default_value(3)
         , "Time after before which the server should giveup witing for auth cmd.")

         ("test,r"
         , po::value<int>(&op.test)->default_value(1)
         , "Which test to run: 1, 2, 3, 4, 5.")
      ;

      po::variables_map vm;        
      po::store(po::parse_command_line(argc, argv, desc), vm);
      po::notify(vm);    

      if (vm.count("help")) {
         std::cout << desc << "\n";
         return 0;
      }

      set_fd_limits(500000);

      switch (op.test)
      {
         case 1: test_online(op); break;
         case 2: test_offline(op); break;
         case 3: read_only_tests(op); break;
         case 4: test_login_error(op); break;
         case 5: test_early_close(op); break;
         default:
            std::cerr << "Invalid test." << std::endl;
      }

   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
      return EXIT_FAILURE;
   }

   return 0;
}

