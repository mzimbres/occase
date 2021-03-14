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

#include "logger.hpp"
#include "notifier.hpp"

struct config {
   int help = 0; // 0: continue, 1: help, -1: error.
   occase::log::level logfilter;
   occase::notifier::config ntf;
};

using namespace occase;

namespace po = boost::program_options;

auto get_cfg(int argc, char* argv[])
{
   config cfg;
   std::string conf_file;
   std::string logfilter_str;

   po::options_description desc("Options");
   desc.add_options()
   ("help,h", "Produces help message.")
   ("config", po::value<std::string>(&conf_file) , "The file containing the configuration.")
   ("ssl-certificate-file", po::value<std::string>(&cfg.ntf.ssl_cert_file))
   ("ssl-private-key-file", po::value<std::string>(&cfg.ntf.ssl_priv_key_file))
   ("ssl-dh-file", po::value<std::string>(&cfg.ntf.ssl_dh_file))
   ("log-level", po::value<std::string>(&logfilter_str)->default_value("notice"))
   ("wait-interval", po::value<int>(&cfg.ntf.wait_interval)->default_value(5))
   ("max-message-size", po::value<int>(&cfg.ntf.max_msg_size)->default_value(2000))
   ("fcm-port", po::value<std::string>(&cfg.ntf.ntf.fcm_port))
   ("fcm-host", po::value<std::string>(&cfg.ntf.ntf.fcm_host))
   ("fcm-target", po::value<std::string>(&cfg.ntf.ntf.fcm_target))
   ("fcm-server-token", po::value<std::string>(&cfg.ntf.ntf.fcm_server_token))
   ("redis-host", po::value<std::string>(&cfg.ntf.redis_host)->default_value("127.0.0.1"))
   ("redis-port", po::value<std::string>(&cfg.ntf.redis_port)->default_value("6379"))
   ("redis-max-pipeline-size", po::value<int>(&cfg.ntf.redis_max_pipeline_size)->default_value(1024))
   ("redis-notify-channel", po::value<std::string>(&cfg.ntf.redis_notify_channel)->default_value("notify"))
   ("redis-tokens-key", po::value<std::string>(&cfg.ntf.redis_tokens_key)->default_value("fcm_tokens"))
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
      return config {1};
   }

   cfg.logfilter = log::to_level<log::level>(logfilter_str);
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
      notifier ntf {cfg.ntf};
      ntf.run();

   } catch (std::exception const& e) {
      log::write(log::level::notice, e.what());
      log::write(log::level::notice, "Exiting with status 1 ...");
      return 1;
   }

   log::write(log::level::notice, "Exiting with status 0 ...");
   return 0;
}

