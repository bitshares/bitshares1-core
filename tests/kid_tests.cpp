#define BOOST_TEST_MODULE KidTests
#include <boost/test/unit_test.hpp>
#include <bts/kid/kid_server.hpp>
#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>
#include <fc/thread/thread.hpp>
#include <fc/filesystem.hpp>
#include <fc/network/ip.hpp>
#include <fc/io/json.hpp>

BOOST_AUTO_TEST_CASE( kid_server )
{
   try{
      bts::kid::server serv;
      auto trustee = fc::ecc::private_key::generate();

      serv.set_trustee( trustee );
      serv.set_data_directory( "kid_data" );
      serv.listen( fc::ip::endpoint( std::string("0.0.0.0"),8989 ) );

      auto dan_master_key = fc::ecc::private_key::generate();
      auto dan_active_key = fc::ecc::private_key::generate();

      signed_name_record rec;
      rec.name = "dan";
      rec.master_key    = dan_master_key.get_public_key();
      rec.active_key    = dan_active_key.get_public_key();
      rec.prev_block_id = serv.head_block_id();
      rec.last_update   = fc::time_point::now();
      rec.first_update  = rec.last_update;
      rec.nonce         = 0;

      rec.sign( dan_master_key );

      FC_ASSERT( serv.update_record( rec ) );

      fc::usleep( fc::seconds(61) );
   } catch ( const fc::exception& e )
   {
      wlog( "${e}", ("e",e.to_detail_string() ) );
      throw;
   }
}
