#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <fstream>
#include <memory>
#include <string>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>

#include "net.hpp"
#include "crypto.hpp"
#include "logger.hpp"
#include "system.hpp"
#include "img_session.hpp"

namespace rt
{

class listener {
private:
   net::io_context ioc {1};
   tcp::acceptor acceptor;
   std::string doc_root;

public:
   listener(unsigned short port, std::string docroot)
   : acceptor {ioc, {net::ip::tcp::v4(), port}}
   , doc_root {std::move(docroot)}
   {}

   void run()
   {
      do_accept();
      ioc.run();
   }

   void do_accept()
   {
      auto handler = [this](auto const& ec, auto socket)
         { on_accept(ec, std::move(socket)); };

      acceptor.async_accept(ioc, handler);
   }

   void on_accept( boost::system::error_code ec
                 , net::ip::tcp::socket socket)
   {
      if (ec) {
         if (ec == net::error::operation_aborted) {
            log( loglevel::info
               , "imgserver: Stopping accepting connections");
            return;
         }

         log(loglevel::debug, "imgserver: on_accept: {1}", ec.message());
      } else {
         std::make_shared<img_session>(std::move(socket), doc_root)->run();
      }

      do_accept();
   }

   void shutdown()
   {
      acceptor.cancel();
   }
};

}

struct server_cfg {
   bool help = false;
   unsigned short port;
   std::string doc_root;
   bool log_on_stderr = false;
   bool daemonize = false;
   std::string pidfile;
   std::string loglevel;
   int number_of_fds = -1;
   int max_listen_connections;
};

namespace po = boost::program_options;

auto get_cfg(int argc, char* argv[])
{
   server_cfg cfg;
   std::string log_on_stderr = "no";
   std::string conf_file;
   std::string daemonize;

   po::options_description desc("Options");
   desc.add_options()
   ("help,h"
   , "Produces help message")

   ("doc-root"
   , po::value<std::string>(&cfg.doc_root)->default_value("/www/data")
   , "Directory where image will be written to and read from.")

   ("config"
   , po::value<std::string>(&conf_file)
   , "The file containing the configuration.")

   ( "port"
   , po::value<unsigned short>(&cfg.port)->default_value(8888)
   , "Server listening port.")

   ("log-on-stderr"
   , po::value<std::string>(&log_on_stderr)->default_value("no")
   , "Instructs syslog to write the messages on stderr as well.")

   ( "log-level"
   , po::value<std::string>(&cfg.loglevel)->default_value("notice")
   , "Control the amount of information that is output in the logs. "
     " Available options are: emerg, alert, crit, err, warning, notice, "
     " info, debug.")

   ("pidfile"
   , po::value<std::string>(&cfg.pidfile)
   , "The pidfile.")

   ( "number-of-fds"
   , po::value<int>(&cfg.number_of_fds)->default_value(-1)
   , "If provided, the server will try to increase the number of file "
     "descriptors to this value, via setrlimit.")

   ("daemonize"
   , po::value<std::string>(&daemonize)->default_value("no")
   , "Runs the server in the backgroud as daemon process.")

   ( "max-listen-connections"
   , po::value<int>(&cfg.max_listen_connections)->default_value(511)
   , "The size of the tcp backlog.")

   ;

   cfg.log_on_stderr = log_on_stderr == "yes";
   cfg.daemonize = daemonize == "yes";

   po::positional_options_description pos;
   pos.add("config", -1);

   po::variables_map vm;        
   po::store(po::command_line_parser(argc, argv).
         options(desc).positional(pos).run(), vm);
   po::notify(vm);    

   if (!std::empty(conf_file)) {
      std::ifstream ifs {conf_file};
      if (ifs) {
         po::store(po::parse_config_file(ifs, desc, true), vm);
         notify(vm);
      }
   }

   if (vm.count("help")) {
      std::cout << desc << "\n";
      return server_cfg {true};
   }

   return cfg;
}

using namespace rt;

int main(int argc, char* argv[])
{
   try {
      auto const cfg = get_cfg(argc, argv);
      if (cfg.help)
         return 0;

      if (cfg.daemonize)
         daemonize();

      init_libsodium();
      logger logg {argv[0], cfg.log_on_stderr};
      log_upto(cfg.loglevel);
      pidfile_mgr pidfile_mgr_ {cfg.pidfile};

      if (cfg.number_of_fds != -1)
         set_fd_limits(cfg.number_of_fds);

      listener lst {cfg.port, cfg.doc_root};
      drop_root_priviledges();
      lst.run();
   } catch(std::exception const& e) {
      log(loglevel::notice, e.what());
      log(loglevel::notice, "Exiting with status 1 ...");
      return 1;
   }
}

