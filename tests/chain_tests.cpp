#define BOOST_TEST_MODULE BlockchainTests2
#include <boost/test/unit_test.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/blockchain/config.hpp>
#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>
#include <fc/io/json.hpp>
#include <fc/thread/thread.hpp>

using namespace bts::blockchain;
using namespace bts::wallet;

const char* test_keys = R"([
  "dce167e01dfd6904015a8106e0e1470110ef2d5b0b18ba7a83cb8204e25c6b5f",
  "7fee1dc3110ba4abe134822c257c9db5beadbe557763cc54e3dc59b699978a50",
  "4e42e82970d3307d26634572cddbf8424c10cee1c8c7fcebdd9417942a08a1bd",
  "e44baa4f693f1bd71ba8f6b8509c56dd41206e2cff07efcf7b42fc79464a1c59",
  "18fd5207b1a0d72465b8f3ed06b44232a366a76f559c5da3aec9f306e179278f",
  "6dc61837b51d0bb80143c1a7ba5c957f122faa96e0a6ff76ba25f9ff42ef69fd",
  "45b0a66b5016618700b4d4fbfa85efd1b0724e8ae95b09a6c9ec850a05254f48",
  "602751d179b75da5432c64a3360a7309609636056c31deae37b8441230c968eb",
  "8ced7ac956e1755e87c21b1b265979c848c79b3bea3b855bb5b819d3baa72ed2",
  "90ef5e50773c90368597e46eaf1b563f76f879aa8969c2e7a2198847f93324c4"
])"; 


BOOST_AUTO_TEST_CASE( genesis_block_test )
{
   try {
      fc::temp_directory dir; 
      chain_database_ptr blockchain = std::make_shared<chain_database>();
      blockchain->open( dir.path() );

      ilog( "." );
      auto delegate_list = blockchain->get_delegates_by_vote();
      ilog( "delegates: ${delegate_list}", ("delegate_list",delegate_list) );
      for( uint32_t i = 0; i < delegate_list.size(); ++i )
      {
         auto name_rec = blockchain->get_name_record( delegate_list[i] );
         ilog( "${i}] ${delegate}", ("i",i) ("delegate", name_rec ) );
      }
      blockchain->scan_names( [=]( const name_record& a ) {
         ilog( "\n${a}", ("a",fc::json::to_pretty_string(a) ) );
      });

      blockchain->scan_assets( [=]( const asset_record& a ) {
         ilog( "\n${a}", ("a",fc::json::to_pretty_string(a) ) );
      });


      wallet  my_wallet( blockchain );
      my_wallet.open( dir.path() / "wallet", "password" );

      auto keys = fc::json::from_string( test_keys ).as<std::vector<fc::ecc::private_key> >();
      for( auto key: keys )
      {
         my_wallet.import_private_key( key );
      }
      my_wallet.scan_state();
      ilog( "next block production time: ${t}", ("t",my_wallet.next_block_production_time()) );

      blockchain->close();
   } 
   catch ( const fc::exception& e )
   {
      elog( "${e}", ("e",e.to_detail_string() ) );
      throw;
   }
   catch ( ... )
   {
      elog( "exception" );
      throw;
   }
}
