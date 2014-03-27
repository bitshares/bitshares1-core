#include <bts/net/chain_server.hpp>
#include <fc/thread/thread.hpp>
#include <fc/log/logger_config.hpp>
#include <fc/io/json.hpp>
#include <fc/crypto/elliptic.hpp>

int main( int argc, char** argv )
{
   try {
       fc::configure_logging( fc::logging_config::default_config() );
       FC_ASSERT( fc::exists( "trustee.key" ) );
       auto key = fc::json::from_file( "trustee.key" ).as<fc::ecc::private_key>();

       bts::net::chain_server cserv;
       cserv.set_trustee_key( key );
       bts::net::chain_server::config cfg;
       cfg.port = 4567;
       cserv.configure(cfg);
       ilog( "sleep..." );
       fc::usleep( fc::seconds( 60*60*24*365 ) );
   } 
   catch ( const fc::exception& e )
   {
     elog( "${e}", ("e",e.to_detail_string() ) );
     return -1;
   }
   ilog( "Exiting normally" );
   return 0;
}
