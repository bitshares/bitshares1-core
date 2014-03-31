#include <bts/kid/kid_server.hpp>
#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>
#include <fc/thread/thread.hpp>
#include <fc/filesystem.hpp>
#include <fc/network/ip.hpp>
#include <fc/io/json.hpp>

int main( int argc, char** argv )
{
   try {
      bts::kid::server serv;

      FC_ASSERT( fc::exists( "trustee.key" ) );
      auto key = fc::json::from_file( "trustee.key" ).as<fc::ecc::private_key>();

      serv.set_trustee( key );
      serv.set_data_directory( "kid_data" );
      serv.listen( fc::ip::endpoint( std::string("0.0.0.0"),8989 ) );

      fc::usleep( fc::seconds(10000) );
   } catch( const fc::exception& e )
   {
      elog( "${e}", ("e",e.to_detail_string() ) );
   }

   return 0;
}
