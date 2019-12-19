#include <thread>
#include <memory>
#include <string>
#include <chrono>
#include <iterator>
#include <algorithm>
#include <fstream>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include <sodium.h>

#include <aedis/aedis.hpp>

#include "net.hpp"
#include "utils.hpp"
#include "logger.hpp"
#include "release.hpp"

using namespace occase;

class notifier {
public:
   struct config {
      std::string fcm_server_token;
      std::string fcm_server_endpoint;
      std::string redis_fcm_token_channel;
      aedis::session::config ss_cfg;
   };

private:
   net::io_context ioc_ {1};
   config cfg_;
   ssl::context& ctx_;
   aedis::session ss_;

   void
   on_db_event( boost::system::error_code ec
              , std::vector<std::string> resp)
   {
      if (ec) {
         log::write( log::level::debug
            , "on_db_event: {1}."
            , ec.message());
      }

      std::copy( std::cbegin(resp)
               , std::cend(resp)
               , std::ostream_iterator<std::string>(std::cout, " "));

      std::cout << std::endl;

      // TODO:: Parse the messages and send the notification to firebase.
   }

   void
   on_db_conn()
   {
      std::cout << "TODO: Send the subscribe." << std::endl;
   }

   void init()
   {
      auto h = [this](auto data, auto const& req)
         { on_db_event(std::move(data), req); };

      ss_.set_msg_handler(h);

      auto g = [this]()
         { on_db_conn(); };

      ss_.set_on_conn_handler(g);
   }

public:
   notifier(config const& cfg, ssl::context& c)
   : cfg_ {cfg}
   , ctx_ {c}
   , ss_ {ioc_, cfg.ss_cfg, "id"}
   {
      init();
   }

   void run()
   {
      ss_.run();
   }
};

struct occase_notify_cfg {
   int help = 0; // 0: continue, 1: help, -1: error.
   std::string ssl_cert_file;
   std::string ssl_priv_key_file;
   std::string ssl_dh_file;
   occase::log::level logfilter;
   notifier::config ntf;

   auto with_ssl() const noexcept
   {
      auto const r = std::empty(ssl_cert_file) ||
                     std::empty(ssl_priv_key_file) ||
                     std::empty(ssl_dh_file);
      return !r;
   }
};

namespace po = boost::program_options;

auto get_cfg(int argc, char* argv[])
{
   occase_notify_cfg cfg;
   std::string conf_file;
   std::vector<std::string> sentinels;
   std::string logfilter_str;
   int max_pipeline_size = 256;

   po::options_description desc("Options");
   desc.add_options()
   ("help,h", "Produces help message. See the config file for explanation.")
   ("git-sha1,v", "The git SHA1 the server was built.")
   ("config", po::value<std::string>(&conf_file) , "The file containing the configuration.")
   ("ssl-certificate-file", po::value<std::string>(&cfg.ssl_cert_file))
   ("ssl-private-key-file", po::value<std::string>(&cfg.ssl_priv_key_file))
   ("ssl-dh-file", po::value<std::string>(&cfg.ssl_dh_file))
   ("log-level", po::value<std::string>(&logfilter_str)->default_value("notice"))
   ;

   po::positional_options_description pos;
   pos.add("config", -1);

   po::variables_map vm;        
   po::store(po::command_line_parser(argc, argv).options(desc).positional(pos).run(), vm);
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
      return occase_notify_cfg {1};
   }

   if (vm.count("git-sha1")) {
      std::cout << GIT_SHA1 << "\n";
      return occase_notify_cfg {1};
   }

   cfg.logfilter = occase::log::to_level<occase::log::level>(logfilter_str);
   return cfg;
}

int main(int argc, char* argv[])
{
   try {
      auto const cfg = get_cfg(argc, argv);
      if (cfg.help == 1)
         return 0;

      if (cfg.help == -1)
         return 1;

      log::upto(cfg.logfilter);

      ssl::context ctx {ssl::context::tlsv12};

      if (cfg.with_ssl()) {
         auto const b =
            load_ssl( ctx
                    , cfg.ssl_cert_file
                    , cfg.ssl_priv_key_file
                    , cfg.ssl_dh_file);
         if (!b)
            return 1;

         notifier ntf {cfg.ntf, ctx};
         ntf.run();
         return 0;
      }

      notifier ntf {cfg.ntf, ctx};
      ntf.run();

   } catch (std::exception const& e) {
      log::write(log::level::notice, e.what());
      log::write(log::level::notice, "Exiting with status 1 ...");
      return 1;
   }

   log::write(log::level::notice, "Exiting with status 0 ...");
   return 0;
}

