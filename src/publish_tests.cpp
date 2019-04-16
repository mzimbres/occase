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

#include "utils.hpp"
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
      , {"Publisher"}
      };
   }

   auto make_sub_cfg(std::vector<login> logins) const
   {
      return launcher_cfg
      { logins
      , std::chrono::milliseconds {launch_interval}
      , {"Replier"}
      };
   }

   auto make_handshake_laucher_op() const
   {
      return launcher_cfg
      { std::vector<login>{300}
      , std::chrono::milliseconds {launch_interval}
      , {"Handshake test"}
      };
   }

   auto make_after_handshake_laucher_op() const
   {
      return launcher_cfg
      { std::vector<login>{300}
      , std::chrono::milliseconds {launch_interval}
      , {"After handshake test."}
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

void test_online( options const& op
                , std::vector<login> pub_logins
                , std::vector<login> sub_logins)
{
   boost::asio::io_context ioc;
   timer t;

   auto const ts = [&t]()
   {
      std::cout << "Starting the timer." << std::endl;
      t.start();
   };

   auto const next = [&pub_logins, &ioc, &op, ts]()
   {
      std::cout << "Starting launching pub." << std::endl;
      auto const s2 = std::make_shared< session_launcher<publisher>
                      >( ioc
                       , publisher_cfg {{}, op.n_repliers}
                       , op.make_session_cf()
                       , op.make_pub_cfg(pub_logins)
                       );

      s2->set_on_end(ts);
      s2->run({});
   };

   std::cout << "Starting launching checkers." << std::endl;
   auto const s = std::make_shared< session_launcher<replier>
                   >( ioc
                    , replier_cfg {{}, op.n_publishers}
                    , op.make_session_cf()
                    , op.make_sub_cfg(sub_logins)
                    );
   
   // When finished launching the listeners we begin to launch the
   // publishers.
   s->set_on_end(next);
   s->run({});

   ioc.run();
   std::cout << "Time elapsed: " << t.get_count_now() << "s" << std::endl;
}

void test_offline(options const& op, login login1, login login2)
{
   boost::asio::io_context ioc;

   using client_type1 = session_shell<publisher2>;

   std::cout << "__________________________________________"
             << std::endl;
   std::cout << "Beginning the offline tests." << std::endl;
   std::cout << "Starting the publisher " << login1 << std::endl;
   auto s1 = 
      std::make_shared<client_type1>( ioc
                                    , op.make_session_cf()
                                    , publisher2_cfg {login1});
   s1->run();
   ioc.run();
   auto post_ids = s1->get_mgr().get_post_ids();
   std::sort(std::begin(post_ids), std::end(post_ids));
   auto const n_post_ids = ssize(post_ids);
   std::cout << "Number of acked posts: " << n_post_ids
             << std::endl;
   std::cout << "Publisher finished." << std::endl;

   //_____________

   std::cout << "Starting the reader " << login2 << std::endl;

   using client_type2 = session_shell<replier>;

   std::make_shared<client_type2>( ioc
                                 , op.make_session_cf()
                                 , replier_cfg {login2, 1}
                                 )->run();
   ioc.restart();
   ioc.run();

   std::cout << "Reader finished." << std::endl;

   //_____________

   using client_type3 = session_shell<msg_pull>;

   std::cout << "Starting the publisher " << login1 << std::endl;
   auto s3 = 
      std::make_shared<client_type3>( ioc
                                    , op.make_session_cf()
                                    , msg_pull_cfg
                                      {login1, n_post_ids}
                                    );
   s3->run();
   ioc.restart();
   ioc.run();
   std::cout << "Publisher finished." << std::endl;

   auto post_ids2 = s3->get_mgr().get_post_ids();
   std::sort(std::begin(post_ids2), std::end(post_ids2));
   if (post_ids2 != post_ids)
      std::cout << "Error" << std::endl;
   else
      std::cout << "Success" << std::endl;
}

void read_only_tests(options const& op)
{
   boost::asio::io_context ioc;

   std::make_shared< session_launcher<handshake_tm>
                   >( ioc
                    , handshake_tm_cfg {}
                    , op.make_session_cf()
                    , op.make_handshake_laucher_op()
                    )->run({});

   std::make_shared< session_launcher<no_handshake_tm>
                   >( ioc
                    , handshake_tm_cfg {}
                    , op.make_session_cf()
                    , op.make_after_handshake_laucher_op()
                    )->run({});

   ioc.run();
}

void test_login_error(options const& op)
{
   using client_type1 = session_shell<login_err>;

   {
      // First test: Here we request a user_id from the server and
      // sets a wrong password to see whether the server refuses to
      // login the user.
      auto l1 = test_reg(op.make_session_cf(), op.n_publishers);
      l1.front().pwd = "Kabuf";

      boost::asio::io_context ioc;
      auto s1 = 
         std::make_shared<client_type1>( ioc, op.make_session_cf()
                                       , login_err_cfg {l1.front()});
      s1->run();
      ioc.run();
      std::cout << "Test Ok: Correct user id, wrong pwd." << std::endl;
   }

   {
      // Second test: Here we request a user_id from the server and
      // sets a wrong password to see whether the server refuses to
      // login the user.
      login invalid {"Kabuf", "Magralha"};

      boost::asio::io_context ioc;
      auto s1 = 
         std::make_shared<client_type1>( ioc, op.make_session_cf()
                                       , login_err_cfg {invalid});
      s1->run();
      ioc.run();
      std::cout << "Test Ok: Invalid id." << std::endl;
   }
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
         , "Which test to run: 1, 2, 3, 4.")
      ;

      po::variables_map vm;        
      po::store(po::parse_command_line(argc, argv, desc), vm);
      po::notify(vm);    

      if (vm.count("help")) {
         std::cout << desc << "\n";
         return 0;
      }

      set_fd_limits(500000);

      if (op.test == 1) {
         auto pub_logins = test_reg(op.make_session_cf(), op.n_publishers);
         auto sub_logins = test_reg(op.make_session_cf(), op.n_repliers);
         test_online(op, std::move(pub_logins), std::move(sub_logins));
         std::cout << "Online tests: Ok." << std::endl;
      } else if (op.test == 2) {
         auto login1 = test_reg(op.make_session_cf(), 1);
         auto login2 = test_reg(op.make_session_cf(), 1);
         test_offline(op, login1.front(), login2.front());
         std::cout << "Offline tests: Ok." << std::endl;
      } else if (op.test == 3) {
         read_only_tests(op);
         std::cout << "Read only tests: Ok." << std::endl;
      } else if (op.test == 4) {
         test_login_error(op);
      } else {
         std::cerr << "Invalid test." << std::endl;
      }

   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
      return EXIT_FAILURE;
   }

   return 0;
}

