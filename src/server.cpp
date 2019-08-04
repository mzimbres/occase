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
#include "listener.hpp"

#include <release.hpp>

using namespace rt;

namespace po = boost::program_options;

auto get_server_op(int argc, char* argv[])
{
   std::vector<std::string> ips;
   listener_cfg cfg;
   int conn_retry_interval = 500;
   std::string redis_db = "0";
   std::string log_on_stderr = "no";
   std::string conf_file;
   std::string daemonize;
   std::string version;

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

   ("pidfile"
   , po::value<std::string>(&cfg.pidfile)
   , "The pidfile.")

   ( "port"
   , po::value<unsigned short>(&cfg.port)->default_value(8080)
   , "Server listening port.")

   ( "stats-server-base-port"
   , po::value<std::string>(&cfg.stats.port)->default_value("9090")
   , "The statistics server base port. Each worker will have its own.")

   ( "workers"
   , po::value<int>(&cfg.number_of_workers)->default_value(1)
   , "The number of worker threads, each"
     " one consuming one thread and having its own io_context."
     " For example, in a CPU with 8 cores this number should lie"
     " around 5. Memory consumption should be considered since "
     " each thread has it own non-shared data structure.")

   ( "login-timeout"
   , po::value<int>(&cfg.login_timeout)->default_value(2)
   , "Login timeout in seconds. Started after the websocket "
     "handshake completes.")

   ( "handshake-timeout"
   , po::value<int>(&cfg.handshake_timeout)->default_value(2)
   , "Handshake timeout in seconds. If the websocket handshake lasts "
     "more than that the socket is shutdown and closed.")

   ( "pong-timeout"
   , po::value<int>(&cfg.pong_timeout)->default_value(2)
   , "Pong timeout in seconds. This is the time the client has to "
     "reply a ping frame sent by the server. If a pong is received "
     "on time a new ping is sent on timer expiration. Otherwise the "
     "connection is closed.")

   ( "close-frame-timeout"
   , po::value<int>(&cfg.close_frame_timeout)->default_value(2)
   , "The time we are willing to wait for an ack to websocket "
     "close frame that has been sent to the client.")

   ( "channel-cleanup-rate"
   , po::value<int>(&cfg.worker.channel.cleanup_rate)->default_value(128)
   , "The rate channels will be  cleaned up if"
     " no publish activity is observed. Incremented on every publication"
     " on the channel.")

   ( "max-msgs-per-channels"
   , po::value<int>(&cfg.worker.channel.max_posts)->default_value(32)
   , "Max number of messages stored per channel. Posting on a"
     " channel that reached this number of messages will cause old"
     " messages to be removed.")

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
   , po::value<int>(&cfg.worker.worker.max_posts_on_sub)->default_value(50)
   , "The maximum number of messages that is allowed to be sent to "
     "the user when he subscribes to his channels.")

   ( "password-size"
   , po::value<int>(&cfg.worker.worker.pwd_size)->default_value(10)
   , "The size of the password sent to the app.")

   ( "redis-server-address"
   , po::value<std::string>(&cfg.worker.db.ss_cfg.host)->
       default_value("127.0.0.1")
   , "Address of the redis server.")

   ( "redis-sentinels"
   , po::value<std::vector<std::string>>(&cfg.worker.db.ss_cfg.sentinels)
   , "A list of sentinel addresses in the form ip1:port1 ip2:port2.")

   ( "redis-server-port"
   , po::value<std::string>(&cfg.worker.db.ss_cfg.port)->
       default_value("6379")
   , "Port where redis server is listening.")

   ( "redis-max-pipeline-size"
   , po::value<int>(&cfg.worker.db.ss_cfg.max_pipeline_size)->
       default_value(10000)
   , "The maximum allowed size of pipelined commands in the redis "
     " session.")

   ( "redis-database"
   , po::value<std::string>(&redis_db)->default_value("0")
   , "The redis database to use: 0, 1, 2 etc.")

   ( "redis-key-menu"
   , po::value<std::string>(&cfg.worker.db.cfg.menu_key)->
        default_value("menu")
   , "Redis key holding the menus in json format.")

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
   , po::value<std::string>(&cfg.worker.db.cfg.user_data_prefix_key)->
        default_value("id")
   , "The prefix to every id holding user data (password for example).")

   ("redis-conn-retry-interval"
   , po::value<int>(&conn_retry_interval)->default_value(500)
   , "Time in milliseconds the redis session should wait before trying "
     "to reconnect to redis in case the connection was lost."
   )

   ("redis-user-msg-exp_time"
   , po::value<int>(&cfg.worker.db.cfg.chat_msg_exp_time)->
        default_value(7 * 24 * 60 * 60)
   , "Expiration time in seconds for redis user message keys."
     " After the time has elapsed the keys will be deleted.")

   ( "redis-offline-chat-msgs"
   , po::value<int>(&cfg.worker.db.cfg.max_offline_chat_msgs)->
        default_value(100)
   , "The maximum number of messages a user is allowed to accumulate "
     " (when he is offline)."
   )

   ( "redis-key-menu-channel"
   , po::value<std::string>(&cfg.worker.db.cfg.menu_channel_key)->
        default_value("menu_channel")
   , "The name of the redis channel where publish commands "
     "are be broadcasted to all workers connected to this channel. "
     "Which may or may not be on the same machine."
   )
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
      return listener_cfg {true};
   }

   if (vm.count("git-sha1")) {
      std::cout << GIT_SHA1 << "\n";
      return listener_cfg {true};
   }

   cfg.log_on_stderr = log_on_stderr == "yes";
   cfg.daemonize = daemonize == "yes";

   cfg.worker.db.cfg.chat_msg_prefix += ":";
   cfg.worker.db.cfg.notify_prefix += redis_db + "__:";
   cfg.worker.db.cfg.user_notify_prefix = cfg.worker.db.cfg.notify_prefix
                                      + cfg.worker.db.cfg.chat_msg_prefix;

   cfg.worker.db.ss_cfg.conn_retry_interval =
      std::chrono::milliseconds {conn_retry_interval};

   cfg.worker.timeouts = cfg.get_timeouts();
   return cfg;
}

int main(int argc, char* argv[])
{
   try {
      auto const cfg = get_server_op(argc, argv);
      if (cfg.help)
         return 0;

      if (cfg.daemonize)
         daemonize();

      logger logg {argv[0], cfg.log_on_stderr};
      log_upto(cfg.loglevel);
      pidfile_mgr pidfile_mgr_ {cfg.pidfile};

      set_fd_limits(500000);

      listener lst {cfg};
      lst.run();

   } catch (std::exception const& e) {
      log(loglevel::notice, e.what());
      log(loglevel::notice, "Exiting with status 1 ...");
      return 1;
   }

   log(loglevel::notice, "Exiting with status 0 ...");
   return 0;
}

