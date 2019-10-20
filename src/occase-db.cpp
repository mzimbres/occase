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
#include "worker.hpp"
#include "release.hpp"
#include "db_ssl_session.hpp"
#include "db_plain_session.hpp"

using namespace rt;

struct server_cfg {
   int help = 0; // 0: continue, 1: help, -1: error.
   bool log_on_stderr = false;
   bool daemonize = false;
   std::string ssl_cert_file;
   std::string ssl_priv_key_file;
   std::string ssl_dh_file;
   std::string pidfile;
   std::string loglevel;

   int number_of_fds = -1;

   worker_cfg worker;

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

namespace po = boost::program_options;

auto get_cfg(int argc, char* argv[])
{
   std::vector<std::string> ips;
   server_cfg cfg;
   int conn_retry_interval = 500;
   std::string log_on_stderr = "no";
   std::string conf_file;
   std::string daemonize;
   std::string redis_host1;
   std::string redis_host2;
   std::vector<std::string> sentinels;

   po::options_description desc("Options");
   desc.add_options()
   ("help,h"
   , "Produces help message")

   ("git-sha1,v"
   , "The git SHA1 the server was built.")

   ("config"
   , po::value<std::string>(&conf_file)
   , "The file containing the configuration.")

   ("log-on-stderr"
   , po::value<std::string>(&log_on_stderr)->default_value("no")
   , "Instructs syslog to write the messages on stderr as well.")

   ("daemonize"
   , po::value<std::string>(&daemonize)->default_value("no")
   , "Runs the server in the backgroud as daemon process.")

   ("ssl-certificate-file", po::value<std::string>(&cfg.ssl_cert_file))
   ("ssl-private-key-file", po::value<std::string>(&cfg.ssl_priv_key_file))
   ("ssl-dh-file", po::value<std::string>(&cfg.ssl_dh_file))

   ("pidfile"
   , po::value<std::string>(&cfg.pidfile)
   , "The pidfile.")

   ( "port"
   , po::value<unsigned short>(&cfg.worker.core.port)->default_value(8080)
   , "Server listening port.")

   ( "max-listen-connections"
   , po::value<int>(&cfg.worker.core.max_listen_connections)->default_value(511)
   , "The size of the tcp backlog.")

   ( "stats-server-base-port"
   , po::value<std::string>(&cfg.worker.stats.port)->default_value("9090")
   , "The statistics server base port.")

   ( "handshake-timeout"
   , po::value<int>(&cfg.handshake_timeout)->default_value(2)
   , "Refer to the documentation in Beast for what its meaning.")

   ( "idle-timeout"
   , po::value<int>(&cfg.idle_timeout)->default_value(2)
   , "Refer to the documentation in Beast for what its meaning.")

   ( "channel-cleanup-rate"
   , po::value<int>(&cfg.worker.channel.cleanup_rate)->default_value(128)
   , "The rate channels will be  cleaned up if"
     " no publish activity is observed. Incremented on every publication"
     " on the channel.")

   ( "max-channels-subscribe"
   , po::value<int>(&cfg.worker.channel.max_sub)->default_value(1024)
   , "The maximum number of channels the user is allowed to subscribe to."
     " Remaining channels will be ignored.")

   ( "log-level"
   , po::value<std::string>(&cfg.loglevel)->default_value("notice")
   , "Control the amount of information that is output in the logs. "
     " Available options are: emerg, alert, crit, err, warning, notice, "
     " info, debug.")

   ( "max-posts-on-subscribe"
   , po::value<int>(&cfg.worker.core.max_posts_on_sub)->default_value(50)
   , "The maximum number of messages that is allowed to be sent to "
     "the user when he subscribes to his channels.")

   ( "password-size"
   , po::value<int>(&cfg.worker.core.pwd_size)->default_value(10)
   , "The size of the password sent to the app.")

   ("img-key", po::value<std::string>(&cfg.worker.core.img_key))

   ( "number-of-fds"
   , po::value<int>(&cfg.number_of_fds)->default_value(-1)
   , "If provided, the server will try to increase the number of file "
     "descriptors to this value, via setrlimit.")

   ( "redis-host1"
   , po::value<std::string>(&redis_host1)->default_value("127.0.0.1:6379")
   , "Address of the first redis server in the format ip:port.")

   ( "redis-host2"
   , po::value<std::string>(&redis_host2)->default_value("127.0.0.1:6379")
   , "Address of the second redis server in the format ip:port.")

   ( "redis-sentinels"
   , po::value<std::vector<std::string>>(&sentinels)
   , "A list of sentinel addresses in the form ip1:port1 ip2:port2.")

   ( "redis-max-pipeline-size"
   , po::value<int>(&cfg.worker.db.ss_cfg1.max_pipeline_size)->
       default_value(10000)
   , "The maximum allowed size of pipelined commands in the redis "
     " session.")

   ( "redis-key-channels"
   , po::value<std::string>(&cfg.worker.db.cfg.channels_key)->
        default_value("channels")
   , "Redis key holding the channels in json format.")

   ( "redis-key-post-id"
   , po::value<std::string>(&cfg.worker.db.cfg.post_id_key)->
        default_value("post_id")
   , "The Key used to store post ids.")

   ( "redis-key-user-id"
   , po::value<std::string>(&cfg.worker.db.cfg.user_id_key)->
        default_value("user_id")
   , "The Key used to store the user id counter.")

   ( "redis-key-chat-msgs-counter"
   , po::value<std::string>(&cfg.worker.db.cfg.chat_msgs_counter_key)->
        default_value("chat_msgs_counter")
   , "The name of the key used to store the number of user messages sent"
     " so far.")

   ( "redis-key-chat-msg-prefix"
   , po::value<std::string>(&cfg.worker.db.cfg.chat_msg_prefix)->
        default_value("msg")
   , "That prefix that will be incorporated in the keys that hold"
     " user messages.")

   ( "redis-key-posts"
   , po::value<std::string>(&cfg.worker.db.cfg.posts_key)->
        default_value("posts")
   , "Redis key used to store posts (in a sorted set).")

   ( "redis-key-user-data-prefix"
   , po::value<std::string>(&cfg.worker.db.cfg.user_data_prefix_key)->default_value("id")
   , "The prefix to every id holding user data (password for example).")

   ("redis-conn-retry-interval"
   , po::value<int>(&conn_retry_interval)->default_value(500)
   , "Time in milliseconds the redis session should wait before trying "
     "to reconnect to redis in case the connection was lost."
   )

   ("redis-user-msg-exp_time"
   , po::value<int>(&cfg.worker.db.cfg.chat_msg_exp_time)->default_value(7 * 24 * 60 * 60)
   , "Expiration time in seconds for redis user message keys."
     " After the time has elapsed the keys will be deleted.")

   ( "redis-offline-chat-msgs"
   , po::value<int>(&cfg.worker.db.cfg.max_offline_chat_msgs)->default_value(100)
   , "The maximum number of messages a user is allowed to accumulate "
     " (when he is offline).")

   ( "redis-key-menu-channel"
   , po::value<std::string>(&cfg.worker.db.cfg.menu_channel_key)->
        default_value("menu_channel")
   , "The name of the redis channel where publish commands "
     "are be broadcasted to all workers connected to this channel. "
     "Which may or may not be on the same machine.")
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
      return server_cfg {1};
   }

   if (vm.count("git-sha1")) {
      std::cout << GIT_SHA1 << "\n";
      return server_cfg {1};
   }

   if (std::size(cfg.worker.core.img_key) != crypto_generichash_KEYBYTES) {
      std::cerr << "Image key has the wrong size." << "\n";
      return server_cfg {-1};
   }

   cfg.log_on_stderr = log_on_stderr == "yes";
   cfg.daemonize = daemonize == "yes";

   cfg.worker.db.cfg.chat_msg_prefix += ":";
   cfg.worker.db.cfg.user_notify_prefix
      = cfg.worker.db.cfg.notify_prefix
      + "0__:"
      + cfg.worker.db.cfg.chat_msg_prefix;


   std::chrono::milliseconds tmp {conn_retry_interval};
   cfg.worker.db.ss_cfg1.conn_retry_interval = tmp;
   cfg.worker.db.ss_cfg2.conn_retry_interval = tmp;

   auto const host1 = split(redis_host1);
   cfg.worker.db.ss_cfg1.host = host1.first;
   cfg.worker.db.ss_cfg1.port = host1.second;

   auto const host2 = split(redis_host2);
   cfg.worker.db.ss_cfg2.host = host2.first;
   cfg.worker.db.ss_cfg2.port = host2.second;

   cfg.worker.db.ss_cfg1.sentinels = sentinels;
   cfg.worker.db.ss_cfg2.sentinels = sentinels;

   cfg.worker.timeouts = cfg.get_timeouts();
   return cfg;
}

auto load_ssl(ssl::context& ctx, server_cfg const& cfg)
{
   boost::system::error_code ec;

   // At the moment we do not have certificate with password.
   ctx.set_password_callback( [](auto n, auto k) { return ""; }
                            , ec);

   if (ec) {
      log(loglevel::emerg, "{}", ec.message());
      return false;
   }

   ec = {};

   ctx.set_options(
      ssl::context::default_workarounds |
      ssl::context::no_sslv2 |
      ssl::context::single_dh_use, ec);

   if (ec) {
      log(loglevel::emerg, "{}", ec.message());
      return false;
   }

   ec = {};

   ctx.use_certificate_chain_file(cfg.ssl_cert_file, ec);

   if (ec) {
      log(loglevel::emerg, "{}", ec.message());
      return false;
   }

   ec = {};

   ctx.use_private_key_file( cfg.ssl_priv_key_file
                           , ssl::context::file_format::pem);

   if (ec) {
      log(loglevel::emerg, "{}", ec.message());
      return false;
   }

   ec = {};

   ctx.use_tmp_dh_file(cfg.ssl_dh_file, ec);

   if (ec) {
      log(loglevel::emerg, "{}", ec.message());
      return false;
   }

   return true;
}

int main(int argc, char* argv[])
{
   try {
      auto const cfg = get_cfg(argc, argv);
      if (cfg.help == 1)
         return 0;

      if (cfg.help == -1)
         return 1;

      if (cfg.daemonize)
         daemonize();

      init_libsodium();
      logger logg {argv[0], cfg.log_on_stderr};
      log_upto(cfg.loglevel);
      pidfile_mgr pidfile_mgr_ {cfg.pidfile};

      if (cfg.number_of_fds != -1)
         set_fd_limits(cfg.number_of_fds);

      ssl::context ctx {ssl::context::tlsv12};

      if (cfg.with_ssl()) {
         if (!load_ssl(ctx, cfg))
            return 1;

         worker<db_ssl_session> db {cfg.worker, ctx};
         drop_root_priviledges();
         db.run();
         return 0;
      }

      worker<db_plain_session> db {cfg.worker, ctx};
      drop_root_priviledges();
      db.run();

   } catch (std::exception const& e) {
      log(loglevel::notice, e.what());
      log(loglevel::notice, "Exiting with status 1 ...");
      return 1;
   }

   log(loglevel::notice, "Exiting with status 0 ...");
   return 0;
}

