#include "notifier.hpp"

#include <chrono>

#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace aedis;

namespace occase
{

notifier::notifier(config const& cfg)
: cfg_ {cfg}
, ss_ {ioc_, cfg.ss, "id"}
{
   init();
}

void notifier::run()
{
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

void notifier::on_rpush(std::string const& key)
{
   if (!cfg_.fcm_valid()) {
      log::write( log::level::debug
                , "rpush notification for {0}."
                , key);
      return;
   }

   auto const match = tokens_.find(key);
   if (match == std::cend(tokens_)) {
      log::write( log::level::debug
                , "on_rpush: No entry for key {0}."
                , key);
      return;
   }

   if (!match->second.has_token()) {
      log::write( log::level::debug
                , "Token for user {0} not found."
                , key);
      return;
   }

   // The token is available, we can launch the timer that if not canceled
   // by a del notification the fcm notification is sent.

   boost::system::error_code ec;
   auto const n =
      match->second.timer.expires_from_now(
            cfg_.get_wait_interval(),
            ec);

   if (ec) {
      log::write( log::level::info
                , "on_rpush: {0}."
                , ec.message());
      return;
   }

   if (n > 0) {
      log::write( log::level::debug
                , "New event for user {0}. Prorogating the timer."
                , key);
   } else {
      log::write( log::level::debug
                , "New event for user {0}"
                , key);

   }

   auto f = [match](auto const& ec)
   {
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

      // TODO: Make to https request.
   };

   match->second.timer.async_wait(f);

   log::write( log::level::debug
             , "Starting timer of {0}."
             , key);
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
                , "Cancel notificaition to {0}."
                , key);
   }
}

void notifier::on_token(std::string const& token)
{
   using value_type = map_type::value_type;

   auto const j = json::parse(token);
   auto const user = j.at("id").get<std::string>();
   auto const value = j.at("token").get<std::string>();

   auto const match =
      tokens_.insert(value_type {
         user,
         notifier_data {
            value, net::steady_timer{ioc_}
         }
      }
   );

   if (!match.second)
      match.first->second.token = value;

   log::write( log::level::debug
             , "User {0} has token {1}."
             , user
             , value);
}

auto
is_ntf( std::vector<std::string> const& resp
      , std::string const& ntf_str)
{
   // We want to check if we received the following notification.
   //
   // pmessage __keyevent@0__:rpush __keyevent@0__:rpush a
   //

   // NOTE: If occase-db ever uses rpush on any key other than for push
   // chat messages, we will have to filter them here, perhaps nased on the
   // key prefix.

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

      // We are subscribed to two kinds of events.
      //
      // rpush:
      // 
      //    Happens when occase-db stores a user message in redis.
      //
      // publish:
      //    
      //    Happens when the user logs in and provides his fcm token, which
      //    occase-db will publish so that we become aware of it.
      //

      if (is_ntf(resp, rpush_str)) {
         on_rpush(resp.back());
      } else if (is_ntf(resp, del_str)) {
         on_del(resp.back());
      } else if (is_token(resp, cfg_.redis_token_channel)) {
         on_token(resp.back());
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
   // TODO: Filter the the reponses to these events in
   // notifier::on_db_event. Add a queue for that.

   ss_.send(psubscribe({rpush_str}));
   ss_.send(subscribe(cfg_.redis_token_channel));
}

void notifier::init()
{
   auto const b =
      load_ssl( ctx_
              , cfg_.ssl_cert_file
              , cfg_.ssl_priv_key_file
              , cfg_.ssl_dh_file);
   if (!b) {
      log::write(log::level::notice, "Unable to load ssl files.");
      //return;
   }

   auto h = [this](auto data, auto const& req)
      { on_db_event(std::move(data), req); };

   ss_.set_msg_handler(h);

   auto g = [this]()
      { on_db_conn(); };

   ss_.set_on_conn_handler(g);
}

}

