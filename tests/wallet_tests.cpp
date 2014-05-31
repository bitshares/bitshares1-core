#define BOOST_TEST_MODULE BlockchainTests2
#include <boost/test/unit_test.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/client/client.hpp>
#include <bts/client/messages.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/time.hpp>
#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>
#include <fc/io/json.hpp>
#include <fc/thread/thread.hpp>
#include <iostream>

using namespace bts::blockchain;
using namespace bts::wallet;
using namespace bts::client;

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

BOOST_AUTO_TEST_CASE( public_key_type_test )
{
   try { 
    auto k1 = fc::ecc::private_key::generate().get_public_key();
    auto k2 = fc::ecc::private_key::generate().get_public_key();
    auto k3 = fc::ecc::private_key::generate().get_public_key();

    FC_ASSERT( public_key_type( std::string( public_key_type(k1) ) ) == k1);
    FC_ASSERT( public_key_type( std::string( public_key_type(k2) ) ) == k2);
    FC_ASSERT( public_key_type( std::string( public_key_type(k3) ) ) == k3);
  } catch ( const fc::exception& e )
   {
      elog( "${e}", ("e",e.to_detail_string()) );
      throw;
   }
}

BOOST_AUTO_TEST_CASE( client_tests )
{
   try {
      std::string password = "123456789";
      fc::temp_directory my_dir;
      fc::temp_directory your_dir;

      auto network = std::make_shared<bts::net::simulated_network>();

      auto my_client = std::make_shared<client>(network);
      //auto my_client = std::make_shared<client>();
      my_client->open( my_dir.path(), "genesis2.json" );

      auto your_client = std::make_shared<client>(network);
      //auto your_client = std::make_shared<client>();
      your_client->open( your_dir.path(), "genesis2.json" );

      my_client->wallet_create( "my_wallet", password );
      my_client->wallet_unlock( fc::seconds(999999999), password );
      your_client->wallet_create( "your_wallet", password );
      your_client->wallet_unlock( fc::seconds(999999999), password );

      auto my_account1 = my_client->wallet_create_account( "account1" );
      std::string wif_key = "5KVZgENbXXvTp3Pvps6uijX84Tka5TQK1vCxXLyx74ir9Hqmvbn";
      my_client->wallet_import_private_key( wif_key, "account1", true /*rescan*/ );
      auto bal = my_client->wallet_get_balance();
      ilog( "${bal}", ("bal",bal ) );
      FC_ASSERT( bal[0].first > 0 );

      my_client->wallet_close();
      my_client->wallet_open("my_wallet");
      my_client->wallet_unlock( fc::seconds(999999999), password );

      bal = my_client->wallet_get_balance();
      ilog( "${bal}", ("bal",bal ) );
      FC_ASSERT( bal[0].first > 0 );

      auto trx = my_client->wallet_register_account( "account1", "account1" ); //variant(), false, "account1" );
      ilog( "----" );
      ilog( "${trx}", ("trx",fc::json::to_pretty_string(trx) ) );
      ilog( "----" );

      my_client->wallet_close();
      my_client->wallet_open("my_wallet");
      my_client->wallet_unlock( fc::seconds(999999999), password );
      auto recv_accounts = my_client->get_wallet()->list_receive_accounts();
      ilog( "receive accounts: ${r}", ("r",recv_accounts) );

      auto next_block_production_time = my_client->get_wallet()->next_block_production_time();
      bts::blockchain::advance_time( (next_block_production_time - bts::blockchain::now()).count()/1000000 );
      auto b = my_client->get_chain()->generate_block(next_block_production_time);
      my_client->get_wallet()->sign_block( b );
      ilog( "block: ${b}", ("b",b));
      ilog( "\n\nBROADCASTING\n\n" );
      my_client->get_node()->broadcast( bts::client::block_message( b ) );

      ilog( "my_client ${info}", ("info", fc::json::to_pretty_string(my_client->get_info()) ));
      ilog( "your_client ${info}", ("info", fc::json::to_pretty_string(your_client->get_info()) ));
      ilog( "registered_names: ${info}", ("info", fc::json::to_pretty_string(your_client->blockchain_list_registered_accounts("",100)) ));


   } catch ( const fc::exception& e )
   {
      elog( "${e}", ("e",e.to_detail_string() ) );
      throw;
   }
}

BOOST_AUTO_TEST_CASE( wallet_tests )
{
   try {
      fc::temp_directory my_dir;
      fc::temp_directory your_dir;
      chain_database_ptr my_blockchain = std::make_shared<chain_database>();
      my_blockchain->open( my_dir.path(), "genesis.json" );
      chain_database_ptr your_blockchain = std::make_shared<chain_database>();
      your_blockchain->open( your_dir.path(), "genesis.json" );
      std::string password = "123456789";

      wallet  my_wallet( my_blockchain );
      my_wallet.set_data_directory( my_dir.path() );
      my_wallet.create(  "my_wallet", password );
      my_wallet.unlock( password );
      wlog( "        closing wallet         " );
      my_wallet.close();
      my_wallet.open( "my_wallet" );
      my_wallet.unlock( password );

      auto result = my_wallet.create_account( "my1" );
      auto result2 = my_wallet.create_account( "my2" );
      ilog( "my1: ${a}", ("a",result) );
      ilog( "my2: ${a}", ("a",result2) );
      my_wallet.close();
      my_wallet.open( "my_wallet" );
      my_wallet.unlock( password );


      wallet  your_wallet( your_blockchain );
      your_wallet.set_data_directory( your_dir.path() );
      your_wallet.create(  "your_wallet", password );
      your_wallet.unlock( password );

      auto your_account2 = your_wallet.create_account( "your1" );
      auto your_account3 = your_wallet.create_account( "your2" );
      auto your_account4 = your_wallet.create_account( "your4" );
      ilog( "your1: ${a}", ("a",result) );
      ilog( "your2: ${a}", ("a",result2) );

      auto keys = fc::json::from_string( test_keys ).as<std::vector<fc::ecc::private_key> >();
      for( auto key: keys )
      {
         my_wallet.import_private_key( key, "my1" );
      }

      my_wallet.scan_state();

      ilog( "Produce Next Block at: ${b}", ("b",my_wallet.next_block_production_time() ) );
      my_wallet.close();
      ilog( "Produce Next Block at: ${b}", ("b",my_wallet.next_block_production_time() ) );
      my_wallet.open( "my_wallet" );
      ilog( "Produce Next Block at: ${b}", ("b",my_wallet.next_block_production_time() ) );

      ilog( "balance: * ${b}", ("b",my_wallet.get_balance() ) );
      ilog( "balance: my1 ${b}", ("b",my_wallet.get_balance("XTS", "my1") ) );
      ilog( "balance: my2 ${b}", ("b",my_wallet.get_balance("XTS", "my2") ) );

      wlog( "adding your4..\n\n" );
      my_wallet.add_contact_account( "your4", your_account4 );
      BOOST_CHECK_THROW( my_wallet.get_balance("XTS", "your4"), fc::exception );

      auto transfer_trxs = my_wallet.transfer( 5000000, "XTS", "my1", "your4", "testa", true );
      ilog( "${trxs}", ("trxs", fc::json::to_pretty_string(transfer_trxs)) );
      if( transfer_trxs.size() > 0 )
         ilog( "size: ${size}", ("size", fc::raw::pack_size( transfer_trxs[0] )) );
      else
         FC_ASSERT( transfer_trxs.size() > 0  );

     

      for( auto trx : transfer_trxs )
      {
        my_blockchain->store_pending_transaction( trx );
      }


      auto next_block_time = my_wallet.next_block_production_time();
      auto sleep_time = next_block_time - bts::blockchain::now(); //fc::time_point::now();
      ilog( "waiting: ${t}s", ("t",sleep_time.count()/1000000) );
      bts::blockchain::advance_time((uint32_t)(sleep_time.count() / 1000000));

      full_block next_block = my_blockchain->generate_block( next_block_time );
      my_wallet.sign_block( next_block );

      my_blockchain->push_block( next_block );
      your_blockchain->push_block( next_block );
      your_wallet.scan_chain(1);
      ilog( "your balance; ${b}", ("b", your_wallet.get_balance() ) );
      your_wallet.close();
      your_wallet.open("your_wallet");

      ilog( "your balance; ${b}", ("b", your_wallet.get_balance() ) );
      ilog( "your1: ${b}", ("b", your_wallet.get_balance("XTS","your1") ) );
      ilog( "your2: ${b}", ("b", your_wallet.get_balance("XTS","your2") ) );
      ilog( "your4: ${b}", ("b", your_wallet.get_balance("XTS","your4") ) );



      //my_wallet.scan_state();

   } catch ( const fc::exception& e )
   {
      elog( "${e}", ("e",e.to_detail_string() ) );
      throw;
   }

}

