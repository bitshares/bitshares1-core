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

void initialize_chain_state( chain_database_ptr blockchain, const std::vector<address>& addresses )
{
   fc::time_point_sec timestamp = fc::time_point::now();
   // create the identities for the initial delegates
   for( uint64_t i = 1; i <= BTS_BLOCKCHAIN_DELEGATES; ++i )
   {
      name_record initial_delegate;
      initial_delegate.id = i;
      initial_delegate.name = "delegate-"+fc::to_string(i);
      initial_delegate.owner_key = fc::ecc::private_key::regenerate( fc::sha256::hash( initial_delegate.name.c_str(), initial_delegate.name.size() ) ).get_public_key().serialize();
      initial_delegate.active_key = initial_delegate.owner_key;
      initial_delegate.last_update = timestamp;
      initial_delegate.is_delegate = true;
      initial_delegate.votes_for   = BTS_BLOCKCHAIN_MAX_SHARES/BTS_BLOCKCHAIN_DELEGATES;

      blockchain->store_name_record( initial_delegate );
   }

   for( uint32_t i = 1; i <= BTS_BLOCKCHAIN_DELEGATES; ++i )
   {
      account_record initial_balance( addresses[i%addresses.size()], 
                                      asset( BTS_BLOCKCHAIN_MAX_SHARES/BTS_BLOCKCHAIN_DELEGATES, 0 ), i );
      blockchain->store_account_record( initial_balance );
   }

   name_record god;
   god.id = 0;
   god.name = "god";

   blockchain->store_name_record( god );

   asset_record base_asset;
   base_asset.id                   = 0;
   base_asset.symbol               = BTS_ADDRESS_PREFIX;
   base_asset.name                 = "BitShares XT";
   base_asset.description          = "The base shares of the DAC";
   base_asset.issuer_name_id       = 0;
   base_asset.maximum_share_supply = BTS_BLOCKCHAIN_MAX_SHARES;
   base_asset.current_share_supply = BTS_BLOCKCHAIN_MAX_SHARES;

   blockchain->store_asset_record( base_asset );
}

BOOST_AUTO_TEST_CASE( genesis_block_test )
{
   try {
      fc::temp_directory dir; 
      chain_database_ptr blockchain = std::make_shared<chain_database>();
      blockchain->open( dir.path() );

      wallet  my_wallet( blockchain );
      my_wallet.open( dir.path() / "wallet", "password" );

      std::vector<address> addresses;

      for( uint32_t i = 0; i < 10; ++i )
         addresses.push_back( my_wallet.get_new_address() );

      initialize_chain_state( blockchain, addresses );


      for( uint64_t i = 1; i <= BTS_BLOCKCHAIN_DELEGATES; ++i )
      {
         std::string delegate_name = "delegate-"+fc::to_string(i);
         my_wallet.import_private_key( 
                    fc::ecc::private_key::regenerate( fc::sha256::hash( delegate_name.c_str(), 
                                                                        delegate_name.size() ) ) );
      }

      my_wallet.scan_state();
      ilog( "balance: ${b}", ("b", my_wallet.get_balance() ) );

      for( uint32_t i = 0; i < 3; ++i )
      {
         auto spend_trx = my_wallet.send_to_address( asset( 400000000, 0 ), address() );
         ilog( "\n${spend_trx}", ("spend_trx", fc::json::to_pretty_string( spend_trx ) ) );

         ilog( "size: ${s}", ("s",spend_trx.data_size() ) );

         auto eval_state = blockchain->store_pending_transaction( spend_trx );
         FC_ASSERT( eval_state );
         ilog( "\n${eval_state}", ("eval_state", fc::json::to_pretty_string( *eval_state ) ) );
      }

      for( uint32_t i = 0; i < 2; ++i )
      {
         auto next_block_time = my_wallet.next_block_production_time();
         ilog( "next block production time: ${t}", ("t",next_block_time) );
         auto wait_time = next_block_time - fc::time_point::now();
         ilog( "waiting ${s} seconds to produce block", ("s", wait_time.count()/1000000 ) );
         fc::usleep( wait_time );
         ilog( "producing block" );
         auto pending_trxs = blockchain->get_pending_transactions();
         auto next_block   = blockchain->generate_block( next_block_time );
         ilog( "produced block:\n${b}", ("b",fc::json::to_pretty_string( next_block ) ) );
         my_wallet.sign_block( next_block );

         blockchain->push_block( next_block );

         /** the wallet can ony perform this operation if it controls the ID of the
          * current delegate.
          */
         // auto next_block = my_wallet.create_next_block( trxs );

         ilog( "balance: ${b}", ("b", my_wallet.get_balance() ) );
         fc::usleep( fc::seconds(2) );
      }

      for( uint32_t i = 0; i < 3; ++i )
      {
         auto spend_trx = my_wallet.send_to_address( asset( 20000000000, 0 ), address() );
         ilog( "\n${spend_trx}", ("spend_trx", fc::json::to_pretty_string( spend_trx ) ) );

         ilog( "size: ${s}", ("s",spend_trx.data_size() ) );

         auto eval_state = blockchain->store_pending_transaction( spend_trx );
         FC_ASSERT( eval_state );
         ilog( "\n${eval_state}", ("eval_state", fc::json::to_pretty_string( *eval_state ) ) );
      }

      for( uint32_t i = 0; i < 1; ++i )
      {
         auto next_block_time = my_wallet.next_block_production_time();
         ilog( "next block production time: ${t}", ("t",next_block_time) );
         auto wait_time = next_block_time - fc::time_point::now();
         ilog( "waiting ${s} seconds to produce block", ("s", wait_time.count()/1000000 ) );
         fc::usleep( wait_time );
         ilog( "producing block" );
         auto pending_trxs = blockchain->get_pending_transactions();
         auto next_block   = blockchain->generate_block( next_block_time );
         ilog( "produced block:\n${b}", ("b",fc::json::to_pretty_string( next_block ) ) );
         my_wallet.sign_block( next_block );

         blockchain->push_block( next_block );

         /** the wallet can ony perform this operation if it controls the ID of the
          * current delegate.
          */
         // auto next_block = my_wallet.create_next_block( trxs );

         ilog( "balance: ${b}", ("b", my_wallet.get_balance() ) );
         fc::usleep( fc::seconds(2) );
      }

      auto name_reg_trx = my_wallet.reserve_name( "dan", nullptr );
      auto eval_state = blockchain->store_pending_transaction( name_reg_trx );
      ilog( "\n${eval_state}", ("eval_state", fc::json::to_pretty_string( *eval_state ) ) );

      {
         auto next_block_time = my_wallet.next_block_production_time();
         ilog( "next block production time: ${t}", ("t",next_block_time) );
         auto wait_time = next_block_time - fc::time_point::now();
         ilog( "waiting ${s} seconds to produce block", ("s", wait_time.count()/1000000 ) );
         fc::usleep( wait_time );
         ilog( "producing block" );
         auto next_block   = blockchain->generate_block( next_block_time );

         my_wallet.sign_block( next_block );
         blockchain->push_block( next_block );


         /** the wallet can ony perform this operation if it controls the ID of the
          * current delegate.
          */
         // auto next_block = my_wallet.create_next_block( trxs );

         ilog( "balance: ${b}", ("b", my_wallet.get_balance() ) );
      }
      bool did_throw = false;
      try {
         auto name_reg_trx = my_wallet.reserve_name( "dan", nullptr );
      } catch ( const fc::exception& e )
      {
         wlog( "${e}", ("e",e.to_detail_string() ) );
         did_throw = true;
      }
      FC_ASSERT( did_throw );





      {
         fc::usleep( fc::seconds(1) );
         auto update_name_reg_trx = my_wallet.update_name( "dan", "cool data" );
         auto eval_state = blockchain->store_pending_transaction( update_name_reg_trx );
         ilog( "\n${eval_state}", ("eval_state", fc::json::to_pretty_string( *eval_state ) ) );

         auto next_block_time = my_wallet.next_block_production_time();
         ilog( "next block production time: ${t}", ("t",next_block_time) );
         auto wait_time = next_block_time - fc::time_point::now();
         ilog( "waiting ${s} seconds to produce block", ("s", wait_time.count()/1000000 ) );
         fc::usleep( wait_time );
         ilog( "producing block" );
         auto next_block   = blockchain->generate_block( next_block_time );

         my_wallet.sign_block( next_block );
         blockchain->push_block( next_block );


         /** the wallet can ony perform this operation if it controls the ID of the
          * current delegate.
          */
         // auto next_block = my_wallet.create_next_block( trxs );

         ilog( "balance: ${b}", ("b", my_wallet.get_balance() ) );
      }











      

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

      ilog( "next block production time: ${t}", ("t",my_wallet.next_block_production_time()) );

      blockchain->close();
   } 
   catch ( const fc::exception& e )
   {
      elog( "${e}", ("e",e.to_detail_string() ) );
      throw;
   }
}
