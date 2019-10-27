#include "net.hpp"

#include "logger.hpp"

namespace rt
{

bool load_ssl( ssl::context& ctx
             , std::string const& ssl_cert_file
             , std::string const& ssl_priv_key_file
             , std::string const& ssl_dh_file)
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
      log( loglevel::emerg
         , "load_ssl: set_options: {}"
         , ec.message());
      return false;
   }

   ec = {};

   ctx.use_certificate_chain_file(ssl_cert_file, ec);

   if (ec) {
      log( loglevel::emerg
         , "load_ssl use_certificate_chain_file: {}"
         , ec.message());
      return false;
   }

   ec = {};

   ctx.use_private_key_file( ssl_priv_key_file
                           , ssl::context::file_format::pem);

   if (ec) {
      log( loglevel::emerg
         , "load_ssl use_private_key_file: {}"
         , ec.message());
      return false;
   }

   ec = {};

   ctx.use_tmp_dh_file(ssl_dh_file, ec);

   if (ec) {
      log( loglevel::emerg
         , "load_ssl use_tmp_dh_file: {}"
         , ec.message());
      return false;
   }

   return true;
}

}

