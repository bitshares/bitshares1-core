#include <bts/net/chain_server.hpp>
#include <fc/thread/thread.hpp>
#include <fc/log/logger_config.hpp>
#include <fc/io/json.hpp>
#include <fc/crypto/elliptic.hpp>

int main( int argc, char** argv )
{
   try {
       fc::configure_logging( fc::logging_config::default_config() );

       bts::net::chain_server cserv;
       bts::net::chain_server::config cfg;
       cfg.port = 4569;
       cserv.configure(cfg);
       cserv.get_chain().set_trustee( bts::blockchain::address( "43cgLS17F2uWJKKFbPoJnnoMSacj" ) );
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
