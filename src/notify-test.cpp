#include "net.hpp"

#include <string>
#include <iostream>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

#include "logger.hpp"
#include "ntf_session.hpp"

using namespace occase;

namespace po = boost::program_options;

struct options {
   int help = 0;
   std::string title;
   std::string body;
   std::string fcm_token;
};

auto parse_options(int argc, char* argv[])
{
   options op;
   std::string of = "tree";
   po::options_description desc("Options");
   desc.add_options()
   ( "help,h", "This help message.")
   ( "title,t", po::value<std::string>(&op.title)->default_value("Test title"), "Message title.")
   ( "body,b", po::value<std::string>(&op.body)->default_value("Test body."), "Message body.")
   ( "fcm-token,c", po::value<std::string>(&op.fcm_token)->default_value("dQ2rmqTJUNclfzqQoR9Xkj:APA91bGJeqWlL4tlgKBrfl26FUBcnao0BjZrrCZ6bVr4HHoJfkod4nA99ZoMtwLcXGjoVq2Akaj7cItE_oH0X6zYHtxP0aNO54l79F5IfAiahjCCmgDinyyCSPdN3OsLrUpPLJQm3JJE"), "User private token.")
   ;

   po::positional_options_description pos;
   pos.add("fcm-token", -1);

   po::variables_map vm;        
   po::store(po::command_line_parser(argc, argv).
      options(desc).positional(pos).run(), vm);
   po::notify(vm);    

   if (vm.count("help")) {
      op.help = 1;
      std::cout << desc << "\n";
      return op;
   }

   return op;
}

int main(int argc, char** argv)
{
   try {
      auto const op = parse_options(argc, argv);
      if (op.help == 1)
         return 0;

      ntf_session::config cfg;

      log::upto(log::level::debug);

      net::io_context ioc {BOOST_ASIO_CONCURRENCY_HINT_UNSAFE};
      ssl::context ctx{ssl::context::tlsv12_client};
      ctx.set_verify_mode(ssl::verify_none);

      auto ntf_body = make_ntf_body(
         op.title,
         op.body,
         op.fcm_token);

      log::write(log::level::info, "Sending: {0}", ntf_body);

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

