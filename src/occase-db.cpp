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

#include "utils.hpp"
#include "crypto.hpp"
#include "logger.hpp"
#include "system.hpp"
#include "release.hpp"
#include "db_worker.hpp"
#include "db_adm_ssl_session.hpp"
#include "db_adm_plain_session.hpp"

using namespace rt;

struct occase_db_cfg {
   int help = 0; // 0: continue, 1: help, -1: error.
   std::string ssl_cert_file;
   std::string ssl_priv_key_file;
   std::string ssl_dh_file;
   rt::loglevel logfilter;

   db_worker_cfg worker;

   int handshake_timeout;
   int idle_timeout;

   auto get_timeouts() const noexcept
   {
      return ws_timeouts
      { std::chrono::seconds {handshake_timeout}
      , std::chrono::seconds {idle_timeout}
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

std::pair<std::string, std::string> split2(std::string data)
{
   auto const pos = data.find_first_of(':');
   if (pos == std::string::npos)
      return {};

   if (1 + pos == std::size(data))
      return {};

   return {data.substr(0, pos), data.substr(pos + 1)};
}

namespace po = boost::program_options;

auto get_cfg(int argc, char* argv[])
{
   std::vector<std::string> ips;
   occase_db_cfg cfg;
   int conn_retry_interval = 500;
   std::string conf_file;
   std::string redis_host1;
   std::string redis_host2;
   std::vector<std::string> sentinels;
   std::string logfilter_str;

   po::options_description desc("Options");
   desc.add_options()
   ("help,h", "Produces help message. See the config file for explanation.")
   ("git-sha1,v", "The git SHA1 the server was built.")
   ("config", po::value<std::string>(&conf_file) , "The file containing the configuration.")
   ("ssl-certificate-file", po::value<std::string>(&cfg.ssl_cert_file))
   ("ssl-private-key-file", po::value<std::string>(&cfg.ssl_priv_key_file))
   ("ssl-dh-file", po::value<std::string>(&cfg.ssl_dh_file))
   ("db-port", po::value<unsigned short>(&cfg.worker.core.db_port)->default_value(8080))
   ("max-listen-connections", po::value<int>(&cfg.worker.core.max_listen_connections)->default_value(511))
   ("adm-password", po::value<std::string>(&cfg.worker.core.adm_pwd))
   ("db-host", po::value<std::string>(&cfg.worker.core.db_host))
   ("handshake-timeout", po::value<int>(&cfg.handshake_timeout)->default_value(2))
   ("idle-timeout", po::value<int>(&cfg.idle_timeout)->default_value(2))
   ("post-expiration", po::value<int>(&cfg.worker.channel.post_expiration)->default_value(2160))
   ("channel-cleanup-rate", po::value<int>(&cfg.worker.channel.cleanup_rate)->default_value(128))
   ("max-channels-subscribe", po::value<int>(&cfg.worker.channel.max_sub)->default_value(1024))
   ("log-level", po::value<std::string>(&logfilter_str)->default_value("notice"))
   ("max-posts-on-subscribe", po::value<int>(&cfg.worker.core.max_posts_on_sub)->default_value(50))
   ("post-interval", po::value<long int>(&cfg.worker.core.post_interval)->default_value(40))
   ("password-size", po::value<int>(&cfg.worker.core.pwd_size)->default_value(10))
   ("http-session-timeout", po::value<int>(&cfg.worker.core.http_session_timeout)->default_value(30))
   ("ssl-shutdown-timeout", po::value<int>(&cfg.worker.core.ssl_shutdown_timeout)->default_value(30))
   ("server-name", po::value<std::string>(&cfg.worker.core.server_name)->default_value("occase-db"))
   ("mms-key", po::value<std::string>(&cfg.worker.core.mms_key))
   ("mms-host", po::value<std::string>(&cfg.worker.core.mms_host))
   ("redis-host1", po::value<std::string>(&redis_host1)->default_value("127.0.0.1:6379"))
   ("redis-host2", po::value<std::string>(&redis_host2)->default_value("127.0.0.1:6379"))
   ("redis-sentinels", po::value<std::vector<std::string>>(&sentinels))
   ("redis-max-pipeline-size", po::value<int>(&cfg.worker.db.ss_cfg1.max_pipeline_size)->default_value(10000))
   ("redis-key-channels", po::value<std::string>(&cfg.worker.db.channels_key)->default_value("channels"))
   ("redis-key-post-id", po::value<std::string>(&cfg.worker.db.post_id_key)->default_value("post_id"))
   ("redis-key-user-id", po::value<std::string>(&cfg.worker.db.user_id_key)->default_value("user_id"))
   ("redis-key-chat-msgs-counter", po::value<std::string>(&cfg.worker.db.chat_msgs_counter_key)->default_value("chat_msgs_counter"))
   ("redis-key-chat-msg-prefix", po::value<std::string>(&cfg.worker.db.chat_msg_prefix)->default_value("msg"))
   ("redis-key-posts", po::value<std::string>(&cfg.worker.db.posts_key)->default_value("posts"))
   ("redis-key-user-data-prefix", po::value<std::string>(&cfg.worker.db.user_data_prefix_key)->default_value("id"))
   ("redis-conn-retry-interval", po::value<int>(&conn_retry_interval)->default_value(500))
   ("redis-user-msg-exp_time", po::value<int>(&cfg.worker.db.chat_msg_exp_time)->default_value(7 * 24 * 60 * 60))
   ("redis-offline-chat-msgs", po::value<int>(&cfg.worker.db.max_offline_chat_msgs)->default_value(100))
   ("redis-key-menu-channel", po::value<std::string>(&cfg.worker.db.menu_channel_key)->default_value("menu_channel"))
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
      return occase_db_cfg {1};
   }

   if (vm.count("git-sha1")) {
      std::cout << GIT_SHA1 << "\n";
      return occase_db_cfg {1};
   }

   if (std::size(cfg.worker.core.mms_key) != crypto_generichash_KEYBYTES) {
      std::cerr << "Image key has the wrong size." << "\n";
      return occase_db_cfg {-1};
   }

   cfg.worker.db.chat_msg_prefix += ":";
   cfg.worker.db.user_notify_prefix
      = cfg.worker.db.notify_prefix
      + "0__:"
      + cfg.worker.db.chat_msg_prefix;


   std::chrono::milliseconds tmp {conn_retry_interval};

   auto const host1 = split2(redis_host1);
   cfg.worker.db.ss_cfg1.conn_retry_interval = tmp;
   cfg.worker.db.ss_cfg1.host = host1.first;
   cfg.worker.db.ss_cfg1.port = host1.second;
   cfg.worker.db.ss_cfg1.sentinels = sentinels;
   cfg.worker.db.ss_cfg1.log_filter =
      to_loglevel<aedis::log::level>(logfilter_str);

   auto const host2 = split2(redis_host2);
   cfg.worker.db.ss_cfg2.conn_retry_interval = tmp;
   cfg.worker.db.ss_cfg2.host = host2.first;
   cfg.worker.db.ss_cfg2.port = host2.second;
   cfg.worker.db.ss_cfg2.sentinels = sentinels;
   cfg.worker.db.ss_cfg2.log_filter =
      to_loglevel<aedis::log::level>(logfilter_str);

   cfg.worker.timeouts = cfg.get_timeouts();
   cfg.logfilter = to_loglevel<rt::loglevel>(logfilter_str);
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
      log_upto(cfg.logfilter);

      ssl::context ctx {ssl::context::tlsv12};

      if (cfg.with_ssl()) {
         auto const b =
            load_ssl( ctx
                    , cfg.ssl_cert_file
                    , cfg.ssl_priv_key_file
                    , cfg.ssl_dh_file);
         if (!b)
            return 1;

         db_worker<db_adm_ssl_session> db {cfg.worker, ctx};
         db.run();
         return 0;
      }

      db_worker<db_adm_plain_session> db {cfg.worker, ctx};
      db.run();

   } catch (std::exception const& e) {
      log(loglevel::notice, e.what());
      log(loglevel::notice, "Exiting with status 1 ...");
      return 1;
   }

   log(loglevel::notice, "Exiting with status 0 ...");
   return 0;
}

