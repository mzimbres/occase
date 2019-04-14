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
#include "client_mgr_pub.hpp"
#include "client_mgr_gmsg_check.hpp"
#include "client_session.hpp"
#include "session_launcher.hpp"

using namespace rt;

namespace po = boost::program_options;

struct client_op {
   std::string host {"127.0.0.1"};
   std::string port {"8080"};
   int n_publishers = 10;
   int n_listeners = 10;
   int handshake_tm = 3;
   int launch_interval = 100;
   int auth_timeout = 3;
   int type = 2;

   auto make_session_cf() const
   {
      return client_session_cf
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
      , {""}
      };
   }

   auto make_sub_cfg(std::vector<login> logins) const
   {
      return launcher_cfg
      { logins
      , std::chrono::milliseconds {launch_interval}
      , {""}
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

void test_pubsub( client_op const& op
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
      auto const s2 = std::make_shared< session_launcher<client_mgr_pub>
                      >( ioc
                       , cmgr_sim_op {{}, op.n_listeners}
                       , op.make_session_cf()
                       , op.make_pub_cfg(pub_logins)
                       );

      s2->set_on_end(ts);
      s2->run({});
   };

   std::cout << "Starting launching checkers." << std::endl;
   auto const s = std::make_shared< session_launcher<client_mgr_gmsg_check>
                   >( ioc
                    , cmgr_gmsg_check_op {{}, op.n_publishers}
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

void test_pub_offline(client_op const& op, login login1, login login2)
{
   boost::asio::io_context ioc;

   using client_type1 = client_session<test_pub>;

   std::cout << "__________________________________________"
             << std::endl;
   std::cout << "Beginning the offline tests." << std::endl;
   std::cout << "Starting the publisher " << login1 << std::endl;
   auto s1 = 
      std::make_shared<client_type1>( ioc
                                    , op.make_session_cf()
                                    , test_pub_cfg {login1});
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

   using client_type2 = client_session<client_mgr_gmsg_check>;

   std::make_shared<client_type2>( ioc
                                 , op.make_session_cf()
                                 , cmgr_gmsg_check_op {login2, 1}
                                 )->run();
   ioc.restart();
   ioc.run();

   std::cout << "Reader finished." << std::endl;

   //_____________

   using client_type3 = client_session<test_msg_pull>;

   std::cout << "Starting the publisher " << login1 << std::endl;
   auto s3 = 
      std::make_shared<client_type3>( ioc
                                    , op.make_session_cf()
                                    , test_msg_pull_cfg
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

int main(int argc, char* argv[])
{
   try {
      client_op op;
      po::options_description desc("Options");
      desc.add_options()
         ("help,h", "Program used to test publish and subscribe.")
         ( "port,p"
         , po::value<std::string>(&op.port)->default_value("8080")
         , "Server port."
         )
         ("ip,d"
         , po::value<std::string>(&op.host)->default_value("127.0.0.1")
         , "Server ip address."
         )

         ("publishers,u"
         , po::value<int>(&op.n_publishers)->default_value(2)
         , "Number of publishers."
         )

         ("listeners,c"
         , po::value<int>(&op.n_listeners)->default_value(10)
         , "Number of listeners."
         )

         ("launch-interval,g"
         , po::value<int>(&op.launch_interval)->default_value(100)
         , "Interval used to launch test clients."
         )

         ("handshake-timeout,k"
         , po::value<int>(&op.handshake_tm)->default_value(3)
         , "Time before which the server should have given "
           "up on the handshake in seconds.")

         ("auth-timeout,l"
         , po::value<int>(&op.auth_timeout)->default_value(3)
         , "Time after before which the server should giveup witing for auth cmd.")

         ("type,r"
         , po::value<int>(&op.type)->default_value(1)
         , "Which test to run: 1, 2."
         )
      ;

      po::variables_map vm;        
      po::store(po::parse_command_line(argc, argv, desc), vm);
      po::notify(vm);    

      if (vm.count("help")) {
         std::cout << desc << "\n";
         return 0;
      }

      set_fd_limits(500000);

      if (op.type == 1) {
         auto pub_logins = test_reg(op.make_session_cf(), op.n_publishers);
         auto sub_logins = test_reg(op.make_session_cf(), op.n_listeners);
         test_pubsub(op, std::move(pub_logins), std::move(sub_logins));
      } else if (op.type == 2) {
         auto login1 = test_reg(op.make_session_cf(), op.n_publishers);
         auto login2 = test_reg(op.make_session_cf(), op.n_listeners);
         test_pub_offline(op, login1.front(), login2.front());
      } else {
         std::cerr << "Invalid run type." << std::endl;
      }

      return 0;
   } catch (std::exception const& e) {
      std::cerr << "Error: " << e.what() << std::endl;
      return EXIT_FAILURE;
   }
}

