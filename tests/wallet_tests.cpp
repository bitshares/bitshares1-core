#define BOOST_TEST_MODULE BlockchainTests2
#include <boost/test/unit_test.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/time.hpp>
#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>
#include <fc/io/json.hpp>
#include <fc/thread/thread.hpp>
#include <iostream>

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

BOOST_AUTO_TEST_CASE( wallet_tests )
{
   try {
      fc::temp_directory my_dir;
      fc::temp_directory your_dir;
      chain_database_ptr my_blockchain = std::make_shared<chain_database>();
      my_blockchain->open( my_dir.path(), "genesis.dat" );
      chain_database_ptr your_blockchain = std::make_shared<chain_database>();
      your_blockchain->open( your_dir.path(), "genesis.dat" );
      std::string password = "123456789";

      wallet  my_wallet( my_blockchain );
      my_wallet.set_data_directory( my_dir.path() );
      my_wallet.create(  "my_wallet", password );
      my_wallet.unlock( password );
      wlog( "        closing wallet         " );
      my_wallet.close();
      my_wallet.open( "my_wallet" );
      my_wallet.unlock( password );

      auto result = my_wallet.create_account( "account1" );
      auto result2 = my_wallet.create_account( "account2" );
      ilog( "account1: ${a}", ("a",result) );
      ilog( "account2: ${a}", ("a",result2) );


      wallet  your_wallet( your_blockchain );
      your_wallet.set_data_directory( your_dir.path() );
      your_wallet.create(  "your_wallet", password );
      your_wallet.unlock( password );

   //   my_wallet.unlock( fc::seconds(999999999), "012345679" );

   } catch ( const fc::exception& e )
   {
      elog( "${e}", ("e",e.to_detail_string() ) );
      throw;
   }

}
