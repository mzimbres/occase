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

#include "config.hpp"
#include "crypto.hpp"
#include "logger.hpp"
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
   bool help;
   unsigned short port;
   std::string doc_root;
};

namespace po = boost::program_options;

auto get_cfg(int argc, char* argv[])
{
   server_cfg cfg;
   std::string conf_file;

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

   return cfg;
}

using namespace rt;

int main(int argc, char* argv[])
{
   try {
      auto const cfg = get_cfg(argc, argv);
      if (cfg.help)
         return 0;

      init_libsodium();

      listener lst {cfg.port, cfg.doc_root};
      lst.run();
   } catch(std::exception const& e) {
      log(loglevel::notice, e.what());
      log(loglevel::notice, "Exiting with status 1 ...");
      return 1;
   }
}

