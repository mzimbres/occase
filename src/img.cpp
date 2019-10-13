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
#include "acceptor_mgr.hpp"

struct server_cfg {
   bool help = false;
   unsigned short port;
   bool log_on_stderr = false;
   bool daemonize = false;
   std::string pidfile;
   std::string loglevel;
   rt::img_session_cfg cfg;
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
   , po::value<std::string>(&cfg.cfg.doc_root)->default_value("/www/data")
   , "Directory where image will be written to and read from.")

   ("config"
   , po::value<std::string>(&conf_file)
   , "The file containing the configuration.")

   ( "port"
   , po::value<unsigned short>(&cfg.port)->default_value(81)
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

   ("img-key"
   , po::value<std::string>(&cfg.cfg.img_key)
   , "See websocket server for information.")

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

      net::io_context ioc {1};
      ssl::context ctx {ssl::context::tlsv12};
      acceptor_mgr<img_session> lst {ioc};
      lst.run(cfg.cfg, ctx, cfg.port, cfg.max_listen_connections);
      drop_root_priviledges();
      ioc.run();
   } catch(std::exception const& e) {
      log(loglevel::notice, e.what());
      log(loglevel::notice, "Exiting with status 1 ...");
      return 1;
   }
}

