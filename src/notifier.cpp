#include "notifier.hpp"

#include <chrono>
#include <fstream>
#include <iterator>
#include <sstream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace aedis;

namespace
{

auto
make_tokens_file(occase::notifier::map_type const& m)
{
   auto f = [](auto init, auto const& o)
   {
      init += o.first;
      init += "\n";
      init += o.second.token;
      init += "\n";
      return init;
   };

   return
      std::accumulate( std::cbegin(m)
                     , std::cend(m)
                     , std::string {}
                     , f);
}

}

namespace occase
{

notifier::notifier(config const& cfg)
: cfg_ {cfg}
, ss_ {ioc_, cfg.ss, "id"}
, tokens_file_timer_ {ioc_}
{
   init();
}

void notifier::run()
{
   tcp::resolver resolver {ioc_};

   boost::system::error_code ec;
   fcm_results_ = resolver.resolve(
      cfg_.ss_args.fcm_host,
      cfg_.ss_args.fcm_port,
      ec);

   if (ec) {
      log::write(
         log::level::debug,
         "run: {0}",
         ec.message());
      return;
   }

   ss_.run();
   ioc_.run();
}

void print(std::vector<std::string> const& resp)
{
   std::copy( std::cbegin(resp)
            , std::cend(resp)
            , std::ostream_iterator<std::string>(std::cout, " "));

   std::cout << std::endl;
}

void notifier::on_message(json j)
{
   auto const key = "chat:" + j.at("to").get<std::string>();

   if (!cfg_.ss_args.fcm_valid()) {
      log::write( log::level::debug
                , "rpush notification for {0}."
                , key);
      return;
   }

   auto const match = tokens_.find(key);
   if (match == std::cend(tokens_)) {
      log::write( log::level::debug
                , "on_message: No entry for key {0}."
                , key);
      return;
   }

   if (!match->second.has_token()) {
      log::write( log::level::debug
                , "Token for user {0} not found."
                , key);
      return;
   }

   match->second.msg = std::move(j);

   // The token is available, we can launch the timer that if not canceled
   // by a del notification the fcm notification is sent.

   auto const n =
      match->second.timer.expires_after(
         cfg_.get_wait_interval());

   if (n > 0) {
      log::write( log::level::debug
                , "New event for user {0}. Prorogating the timer."
                , key);
   } else {
      log::write( log::level::debug
                , "New event for user {0}"
                , key);

   }

   auto f = [this, match](auto const& ec) noexcept
      { on_timeout(ec, match); };

   match->second.timer.async_wait(f);

   log::write( log::level::debug
             , "Starting timer of {0}."
             , key);
}

void notifier::on_timeout(
   boost::system::error_code ec,
   map_type::const_iterator match) noexcept
{
   try {
      if (ec == boost::asio::error::operation_aborted) {
         // Timer was cancelled, this means occase-db has retrieved the
         // message from redis on time. We have nothing to do.
         return;
      }

      // The timer fired and we can send the notification to the user e.g.
      // via fcm or whatever.
      log::write( log::level::debug
                , "Sending notification to user {0} with token {1}."
                , match->first
                , match->second.token);

      auto msg = match->second.msg.at("msg").get<std::string>();
      if (std::size(msg) > cfg_.max_msg_size)
         msg.resize(cfg_.max_msg_size);

      auto ntf_body = make_ntf_body(
         match->second.msg.at("nick").get<std::string>(),
         msg,
         match->second.token);

      std::make_shared<ntf_session>(ioc_, ctx_)->run(
         cfg_.ss_args,
         fcm_results_,
         std::move(ntf_body));
   } catch (std::exception const& e) {
      log::write(log::level::debug, "Exception caught.");
   }
}

void notifier::on_del(std::string const& key)
{
   auto const match = tokens_.find(key);
   if (match == std::cend(tokens_)) {
      log::write( log::level::debug
                , "on_del: No entry for key {0}."
                , key);
      return;
   }

   auto const n = match->second.timer.cancel();
   if (n > 0) {
      log::write( log::level::debug
                , "Notificaition to {0} has been canceled."
                , key);
   }
}

void notifier::on_publish(std::string const& token)
{
   using value_type = map_type::value_type;

   // We receive two types on messages in these channel, user messages in
   // the form
   //
   // { "cmd":"message"
   // , "from":"6"
   // , "is_sender_post":true
   // , "msg":"g"
   // , "nick":"uuuuu"
   // , "post_id":4
   // , "refers_to":-1
   // , "to":"7"
   // , "type":"chat"
   // }
   //
   // and messages containing tokens
   //
   // { "id":"chat:6"
   // , "token":"jdjdjdjdj"
   // }

   auto const j = json::parse(token);

   auto const cmd = j.at("cmd").get<std::string>();

   if (cmd == "message")
      return on_message(std::move(j));

   if (cmd == "token") {
      auto const user = j.at("id").get<std::string>();
      auto const value = j.at("token").get<std::string>();
      if (std::empty(value)) {
         tokens_.erase(user);
         return;
      }

      auto const match =
         tokens_.insert(value_type {
            user,
            user_entry
            { value
            , token
            , net::steady_timer {ioc_}
            }
         }
      );

      if (!match.second)
         match.first->second.token = value;

      using namespace std::chrono;

      auto const now = system_clock::now().time_since_epoch();
      auto const expiry = tokens_file_timer_.expiry().time_since_epoch();
      if (expiry < now) {
         auto f = [this](auto ec)
         {
            assert(ec != boost::asio::error::operation_aborted);
            std::ofstream ofs {cfg_.tokens_file};
            ofs << make_tokens_file(tokens_);
         };

         auto const n =
            tokens_file_timer_.expires_after(
               cfg_.get_tokens_write_interval());

         assert(n == 0);

         tokens_file_timer_.async_wait(f);
      }

      log::write( log::level::debug
                , "User {0} has token: {1}."
                , user
                , value);
      return;
   }
}

auto
is_ntf( std::vector<std::string> const& resp
      , std::string const& ntf_str)
{
   // We want to check events in the following form
   //
   // pmessage __keyevent@0__:rpush __keyevent@0__:rpush a
   //

   if (std::size(resp) != 4)
      return false;

   if (resp[1] == ntf_str)
      return true;

   return false;
}

auto
is_token( std::vector<std::string> const& resp
        , std::string const& channel )
{
   // The messages from the tokens channel have the following form
   //
   //    message tokens aaaaaaa
   //

   if (std::size(resp) != 3)
      return false;

   if (resp[1] == channel)
      return true;

   return false;
}

void notifier::on_db_event(
   boost::system::error_code ec,
   std::vector<std::string> resp)
{
   try {
      if (ec) {
         log::write( log::level::debug
                   , "on_db_event: {1}."
                   , ec.message());
         return;
      }

      if (is_ntf(resp, del_str)) {
         on_del(resp.back());
      } else if (is_token(resp, cfg_.redis_token_channel)) {
         on_publish(resp.back());
      } else {
         log::write( log::level::notice
                   , "on_db_event: Unknown redis event.");
      }
   } catch (std::exception const& e) {
      log::write( log::level::notice
                , "on_db_event exception: {0}"
                , e.what());
   }
}

void notifier::on_db_conn()
{
   // TODO: Filter the reponses to the commands below in
   // notifier::on_db_event. Add a queue for that.

   ss_.send(psubscribe({del_str}));
   ss_.send(subscribe(cfg_.redis_token_channel));
}

void notifier::init()
{
   ctx_.set_verify_mode(ssl::verify_none);

   auto h = [this](auto data, auto const& req)
      { on_db_event(std::move(data), req); };

   ss_.set_msg_handler(h);

   auto g = [this]()
      { on_db_conn(); };

   ss_.set_on_conn_handler(g);

   // Load the file with the tokens.
   std::vector<std::string> tmp;

   std::ifstream ifs {cfg_.tokens_file};
   std::string buffer;
   while (std::getline(ifs, buffer))
      tmp.push_back(buffer);

   auto const n = std::size(tmp);
   if (n < 2)
      return;

   using value_type = map_type::value_type;
   for (unsigned i = 0; i < n / 2; ++i) {
      auto v = value_type
      { tmp.at(2 * i)
      , user_entry { tmp[2 * i + 1], {}
                   , net::steady_timer {ioc_}}
      };

      tokens_.insert(std::move(v));
   }
}

}

