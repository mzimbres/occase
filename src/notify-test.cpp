#include "net.hpp"

#include <string>
#include <iostream>

#include "logger.hpp"
#include "ntf_session.hpp"

using namespace occase;

int main(int argc, char** argv)
{
   try {
      ntf_session::args cfg;

      std::string fcm_user_token = 
      {"eWyJqxRvo4M:APA91bE1pGVkhDramaP90PXVXqrRSRDPJ26LfcuzV292ouxNjbRcdGkPjC2D3tR6tpz7wCI9h94E6EdJ8Glqs-5Xqrpipp7EGex-GW2oh-oaMN3bOFZ45B1DkhwLBrrf3s9_JRE3j_ko"};

      log::upto(log::level::debug);

      net::io_context ioc {BOOST_ASIO_CONCURRENCY_HINT_UNSAFE};
      ssl::context ctx{ssl::context::tlsv12_client};
      ctx.set_verify_mode(ssl::verify_none);

      auto ntf_body = make_ntf_body(
         "Nova mensagem",
         "Vai ver se eu estou na esquina.",
         fcm_user_token);

      tcp::resolver resolver_ {ioc};
      boost::system::error_code ec;
      auto res = resolver_.resolve(cfg.fcm_host, cfg.fcm_port, ec);
      if (ec) {
         log::write(log::level::debug, "resolve: {0}", ec.message());
         return EXIT_FAILURE;
      }

      std::make_shared<ntf_session
                      >(ioc, ctx)->run(cfg, res, std::move(ntf_body));
      ioc.run();
      return 0;
   } catch (std::exception const& e) {
      log::write(log::level::info, "{0}", e.what());
      return EXIT_FAILURE;
   }
}

