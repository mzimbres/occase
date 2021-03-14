#include "notifier.hpp"

#include <chrono>
#include <fstream>
#include <iterator>
#include <sstream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace occase
{

auto make_tokens_file_content(notifier::map_type const& m)
{
   auto f = [](auto init, auto const& o)
   {
      init += o.first;
      init += "\n";
      init += o.second.token;
      init += "\n";
      return init;
   };

   return std::accumulate(std::cbegin(m), std::cend(m), std::string {}, f);
}

notifier::notifier(config const& cfg)
: cfg_ {cfg}
{
   // I believe this should be called in net::post();
   init();
}

void notifier::run()
{
   tcp::resolver resolver {ioc_};

   boost::system::error_code ec;
   fcm_results_ = resolver.resolve(
      cfg_.ntf.fcm_host,
      cfg_.ntf.fcm_port,
      ec);

   if (ec) {
      log::write(
         log::level::debug,
         "run: {0}",
         ec.message());
      return;
   }

   ioc_.run();
}

void print(std::vector<std::string> const& resp)
{
   std::copy( std::cbegin(resp)
            , std::cend(resp)
            , std::ostream_iterator<std::string>(std::cout, " "));

   std::cout << std::endl;
}

void notifier::on_ntf_message(json j)
{
   auto const to = j.at("to").get<std::string>();

   if (!cfg_.ntf.is_fcm_valid()) {
      log::write( log::level::debug
                , "rpush notification for {0}."
                , to);
      return;
   }

   auto const match = tokens_.find(to);
   if (match == std::cend(tokens_)) {
      log::write( log::level::debug
                , "on_ntf_message: No entry for user_id {0}."
                , to);
      return;
   }

   if (!match->second.has_token()) {
      log::write( log::level::debug
                , "Token for user_id {0} not found."
                , to);
      return;
   }

   match->second.msg = std::move(j);

   // The token is available, we can launch the timer that if not
   // canceled by a del notification the fcm notification is sent.

   auto const n =
      match->second.timer.expires_after(
         cfg_.get_wait_interval());

   if (n > 0) {
      log::write( log::level::debug
                , "New message for user_id {0}. Canceling old timer."
                , to);
   } else {
      log::write( log::level::debug
                , "New message for user_id {0}."
                , to);

   }

   auto f = [this, match](auto const& ec) noexcept
      { on_timeout(ec, match); };

   match->second.timer.async_wait(f);

   log::write( log::level::debug
             , "Starting timer of {0}."
             , to);
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
         cfg_.ntf,
         fcm_results_,
         std::move(ntf_body));

   } catch (std::exception const& e) {
      log::write(log::level::debug, "Exception caught.");
   }
}

void notifier::on_ntf_del(std::string const& key)
{
   auto const match = tokens_.find(key);
   if (match == std::cend(tokens_)) {
      log::write( log::level::debug
                , "on_ntf_del: No entry for key {0}."
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

void notifier::on_ntf_publish(std::string const& payload)
{
   using value_type = map_type::value_type;

   // We receive two types on messages in these channel, user messages
   // in the form
   //
   // { "cmd":"message"
   // , "from":"6"
   // , "is_sender_post":true
   // , "msg":"g"
   // , "nick":"uuuuu"
   // , "post_id":4
   // , "refers_to":-1
   // , "to":"some-user-id"
   // , "type":"chat"
   // }
   //
   // and messages containing tokens
   //
   // { "cmd": "token"
   // , "user_id": "some-user-id"
   // , "token": "some-fcm-token"
   // }

   auto j = json::parse(payload);

   auto const cmd = j.at("cmd").get<std::string>();

   if (cmd == "message")
      return on_ntf_message(std::move(j));

   if (cmd == "token") {
      auto const user_id = j.at("user_id").get<std::string>();
      auto const token = j.at("token").get<std::string>();

      if (std::empty(token)) {
         tokens_.erase(user_id); // The user has disabled notifications.
         return;
      }

      auto const match = tokens_.insert(
	 value_type
	 { user_id
	 , user_entry {token, {}, net::steady_timer {ioc_}}
	 });

      if (!match.second) {
	 // We already have one entry for this user. Reset to the
	 // newest one.
         match.first->second.token = token;
      }

      log::write( log::level::debug
                , "User {0} has token: {1}."
                , user_id
                , token);
      return;
   }
}

auto
is_notification( std::vector<std::string> const& resp
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
        , std::string const& channel)
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

void notifier::on_push(aedis::resp::array_type& v) noexcept
{
   try {
      if (is_notification(v, redis_del_ntf)) {
         on_ntf_del(v.back());
      } else if (is_token(v, cfg_.redis_notify_channel)) {
         on_ntf_publish(v.back());
      } else {
         log::write( log::level::notice
                   , "on_push: Unknown redis event.");
      }
   } catch (std::exception const& e) {
      log::write( log::level::notice
                , "on_push exception: {0}"
                , e.what());
   }
}

void notifier::on_hgetall(aedis::resp::array_type& tokens) noexcept
{
   using value_type = map_type::value_type;

   auto const n = std::size(tokens) / 2;

   for (auto i = 0u; i < n; ++i) {
      auto const match = tokens_.insert(
         value_type
         { tokens[2 * i + 0]
         , user_entry {tokens[2 * i + 1], {}, net::steady_timer {ioc_}}
         });
   }

   log::write( log::level::debug
	     , "Tokens map size {0}."
	     , std::size(tokens_));
}

void notifier::init()
{
   ctx_.set_verify_mode(ssl::verify_none);

   net::ip::tcp::resolver redis_resv{ioc_};
   auto const res = redis_resv.resolve(cfg_.redis_host, cfg_.redis_port);
   redis_conn_->start(*this, res);

   auto f = [this](aedis::request& req)
   {
      req.psubscribe({redis_del_ntf});
      req.subscribe(cfg_.redis_notify_channel);
      req.hgetall(cfg_.redis_tokens_key);
   };

   redis_conn_->send(f);
}

} // occase

