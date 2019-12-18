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
#include "mms_session.hpp"
#include "acceptor_mgr.hpp"

struct server_cfg {
   bool help = false;
   unsigned short port;
   occase::log::level logfilter;
   occase::mms_session_cfg session_cfg;
   int max_listen_connections;
};

namespace po = boost::program_options;

auto get_cfg(int argc, char* argv[])
{
   server_cfg cfg;
   std::string conf_file;
   std::string logfilter_str;

   po::options_description desc("Options");
   desc.add_options()
   ("help,h"
   , "Produces help message")

   ("doc-root"
   , po::value<std::string>(&cfg.session_cfg.doc_root)->default_value("/www/data")
   , "Directory where image will be written to and read from.")

   ("body-limit"
   , po::value<std::uint64_t>(&cfg.session_cfg.body_limit)->default_value(1000000))

   ("config"
   , po::value<std::string>(&conf_file)
   , "The file containing the configuration.")

   ( "port"
   , po::value<unsigned short>(&cfg.port)->default_value(81)
   , "Server listening port.")

   ( "log-level"
   , po::value<std::string>(&logfilter_str)->default_value("debug")
   , "Control the amount of information that is output in the logs. "
     " Available options are: emerg, alert, crit, err, warning, notice, "
     " info, debug.")

   ("mms-key"
   , po::value<std::string>(&cfg.session_cfg.mms_key)
   , "See websocket server for information.")

   ( "max-listen-connections"
   , po::value<int>(&cfg.max_listen_connections)->default_value(511)
   , "The size of the tcp backlog.")

   ;

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

   cfg.logfilter = occase::log::to_level<occase::log::level>(logfilter_str);
   return cfg;
}

using namespace occase;

int main(int argc, char* argv[])
{
   try {
      auto const cfg = get_cfg(argc, argv);
      if (cfg.help)
         return 0;

      init_libsodium();
      log::upto(cfg.logfilter);

      net::io_context ioc {1};
      ssl::context ctx {ssl::context::tlsv12};
      acceptor_mgr<mms_session> lst {ioc};
      mms_worker worker {cfg.session_cfg};

      lst.run( worker
             , ctx
             , cfg.port
             , cfg.max_listen_connections);

      ioc.run();
   } catch(std::exception const& e) {
      log::write(log::level::notice, e.what());
      log::write(log::level::notice, "Exiting with status 1 ...");
      return 1;
   }
}

