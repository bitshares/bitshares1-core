#include <fc/crypto/elliptic.hpp>
#include <fc/io/json.hpp>
#include <fc/log/logger_config.hpp>
#include <fc/thread/thread.hpp>

#include <bts/net/chain_server.hpp>
#include <bts/dns/dns_db.hpp>

int main( int argc, char** argv )
{
   try {
       fc::configure_logging( fc::logging_config::default_config() );

       bts::blockchain::chain_database_ptr db = std::make_shared<bts::dns::dns_db>();
       bts::net::chain_server cserv(db);
       bts::net::chain_server::config cfg;
       cfg.port = 4567;
       cserv.configure(cfg);
       // disable setting trustee with a bad address... replace with a valid trustee address
       // cserv.get_chain().set_trustee( bts::blockchain::address( "43cgLS17F2uWJKKFbPoJnnoMSacj" ) );
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
