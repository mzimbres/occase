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

#include "crypto.hpp"
#include "logger.hpp"
#include "system.hpp"
#include "release.hpp"
#include "worker.hpp"

using namespace occase;

struct config_all {
   int help = 0; // 0: continue, 1: help, -1: error.
   std::string ssl_cert_file;
   std::string ssl_priv_key_file;
   std::string ssl_dh_file;
   occase::log::level logfilter;

   config::core core;

   int handshake_timeout;
   int idle_timeout;
   int post_interval;
   int post_expiration;

   auto get_timeouts() const noexcept
   {
      return config::timeouts
      { std::chrono::seconds {handshake_timeout}
      , std::chrono::seconds {idle_timeout}
      , std::chrono::seconds {post_interval}
      , std::chrono::seconds {post_expiration}
      };
   }

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
   config_all cfg;
   std::string conf_file;
   std::string logfilter_str;

   po::options_description desc("Options");
   desc.add_options()
   ("help,h", "Produces help message. See the config file for explanation.")
   ("git-sha1,v", "The git SHA1 the server was built.")
   ("config", po::value<std::string>(&conf_file) , "The file containing the configuration.")
   ("ssl-certificate-file", po::value<std::string>(&cfg.ssl_cert_file))
   ("ssl-private-key-file", po::value<std::string>(&cfg.ssl_priv_key_file))
   ("ssl-dh-file", po::value<std::string>(&cfg.ssl_dh_file))
   ("db-port", po::value<unsigned short>(&cfg.core.db_port)->default_value(443))
   ("max-listen-connections", po::value<int>(&cfg.core.max_listen_connections)->default_value(511))
   ("adm-password", po::value<std::string>(&cfg.core.adm_pwd))
   ("db-host", po::value<std::string>(&cfg.core.db_host))
   ("handshake-timeout", po::value<int>(&cfg.handshake_timeout)->default_value(2))
   ("idle-timeout", po::value<int>(&cfg.idle_timeout)->default_value(30))
   ("post-expiration", po::value<int>(&cfg.post_expiration)->default_value(3 * 30 * 24 * 60 * 60))
   ("log-level", po::value<std::string>(&logfilter_str)->default_value("notice"))
   ("max-posts-on-search", po::value<int>(&cfg.core.max_posts_on_search)->default_value(300))
   ("post-interval", po::value<int>(&cfg.post_interval)->default_value(7 * 24 * 60 * 60))
   ("allowed-posts", po::value<int>(&cfg.core.allowed_posts)->default_value(1))
   ("password-size", po::value<int>(&cfg.core.pwd_size)->default_value(8))
   ("http-session-timeout", po::value<int>(&cfg.core.http_session_timeout)->default_value(30))
   ("http-allow-origin", po::value<std::string>(&cfg.core.http_allow_origin)->default_value("*"))
   ("ssl-shutdown-timeout", po::value<int>(&cfg.core.ssl_shutdown_timeout)->default_value(30))
   ("server-name", po::value<std::string>(&cfg.core.server_name)->default_value("occase-db"))
   ("mms-key", po::value<std::string>(&cfg.core.mms_key))
   ("mms-host", po::value<std::string>(&cfg.core.mms_host))
   ("redis-port", po::value<std::string>(&cfg.core.redis.port)->default_value("6379"))
   ("redis-host", po::value<std::string>(&cfg.core.redis.host)->default_value("127.0.0.1"))
   ("redis-key-chat-msgs-counter", po::value<std::string>(&cfg.core.redis.chat_msgs_counter_key)->default_value("chat_msgs_counter"))
   ("redis-key-chat-msg-prefix", po::value<std::string>(&cfg.core.redis.chat_msg_prefix)->default_value("msg"))
   ("redis-key-posts", po::value<std::string>(&cfg.core.redis.posts_key)->default_value("posts"))
   ("redis-user-msg-exp_time", po::value<int>(&cfg.core.redis.chat_msg_exp_time)->default_value(7 * 24 * 60 * 60))
   ("redis-offline-chat-msgs", po::value<int>(&cfg.core.redis.max_offline_chat_msgs)->default_value(100))
   ("redis-key-posts-channel", po::value<std::string>(&cfg.core.redis.posts_channel_key)->default_value("posts_channel"))
   ("redis-notify-channel", po::value<std::string>(&cfg.core.redis.notify_channel)->default_value("notify"))
   ("redis-tokens-key", po::value<std::string>(&cfg.core.redis.tokens_key)->default_value("fcm_tokens"))
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
      return config_all {1};
   }

   if (vm.count("git-sha1")) {
      std::cout << GIT_SHA1 << "\n";
      return config_all {1};
   }

   if (std::size(cfg.core.mms_key) != crypto_generichash_KEYBYTES) {
      std::cerr << "Image key has the wrong size." << "\n";
      return config_all {-1};
   }

   cfg.core.redis.chat_msg_prefix += ":";
   cfg.core.redis.user_notify_prefix
      = cfg.core.redis.notify_prefix
      + "0__:"
      + cfg.core.redis.chat_msg_prefix;

   cfg.core.timeouts = cfg.get_timeouts();
   cfg.logfilter = log::to_level<occase::log::level>(logfilter_str);

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

      init_libsodium();
      log::upto(cfg.logfilter);

      ssl::context ctx {ssl::context::tlsv12};

      if (cfg.with_ssl()) {
         auto const b =
            load_ssl( ctx
                    , cfg.ssl_cert_file
                    , cfg.ssl_priv_key_file
                    , cfg.ssl_dh_file);

         if (!b) {
            log::write(log::level::notice, "Unable to load ssl files.");
            return 1;
         }
      }

      worker db {cfg.core, ctx};
      db.run();

   } catch (std::exception const& e) {
      log::write(log::level::notice, e.what());
      log::write(log::level::notice, "Exiting with status 1 ...");
      return 1;
   }

   log::write(log::level::notice, "Exiting with status 0 ...");
   return 0;
}

