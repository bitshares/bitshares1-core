#define BOOST_TEST_MODULE BlockchainTests2cc
#include <boost/test/unit_test.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/blockchain/genesis_config.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/client/client.hpp>
#include <bts/client/messages.hpp>
#include <bts/cli/cli.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/time.hpp>
#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>
#include <fc/log/file_appender.hpp>
#include <fc/log/logger_config.hpp>
#include <fc/io/json.hpp>
#include <fc/thread/thread.hpp>
#include <bts/utilities/key_conversion.hpp>

#include <fc/network/http/connection.hpp>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <iostream>
#include <fstream>

#define infolog( FORMAT, ... ) \
do { \
std::cerr << "Info: " <<FORMAT << "\n"; \
} while (0)

#define commandlog( FORMAT, ... ) \
do { \
std::cerr << "Command: " << FORMAT << "\n"; \
} while (0)

#define outlog( FORMAT, ... ) \
do { \
std::cerr << "Out: " << FORMAT << "\n"; \
} while (0)

#define run_cmd( client, cmd ) \
do { \
std::cerr << "Command in " << #client << ": " << cmd << "\n"; \
std::cerr << "Out: \n" << client->execute_command_line( cmd ) << "\n"; \
} while (0)


using namespace bts::blockchain;
using namespace bts::wallet;
using namespace bts::utilities;
using namespace bts::client;
using namespace bts::cli;

template<typename T>
void produce_block( T my_client )
{
   infolog( "--------------- produce block ------------------" );
   auto head_num = my_client->get_chain()->get_head_block_num();
   auto next_block_production_time = my_client->get_wallet()->next_block_production_time();
   bts::blockchain::advance_time( (int32_t)((next_block_production_time - bts::blockchain::now()).count()/1000000) );
   auto b = my_client->get_chain()->generate_block(next_block_production_time);
   my_client->get_wallet()->sign_block( b );
   my_client->get_node()->broadcast( bts::client::block_message( b ) );
   FC_ASSERT( head_num+1 == my_client->get_chain()->get_head_block_num() );
}


struct WFixture
{
   WFixture()
   {
      fc::logging_config cfg;
      fc::configure_logging( cfg );

      bts::blockchain::start_simulated_time(fc::time_point::from_iso_string( "20200101T000000" ));

      vector<fc::ecc::private_key> delegate_private_keys;

      genesis_block_config config;
      config.precision         = BTS_BLOCKCHAIN_PRECISION;
      config.timestamp         = bts::blockchain::now();
      config.base_symbol       = BTS_BLOCKCHAIN_SYMBOL;
      config.base_name         = BTS_BLOCKCHAIN_NAME;
      config.base_description  = BTS_BLOCKCHAIN_DESCRIPTION;
      config.supply            = BTS_BLOCKCHAIN_INITIAL_SHARES;

      for( uint32_t i = 0; i < BTS_BLOCKCHAIN_NUM_DELEGATES; ++i )
      {
         name_config delegate_account;
         delegate_account.name = "delegate" + fc::to_string(i);
         delegate_private_keys.push_back( fc::ecc::private_key::generate() );
         auto delegate_public_key = delegate_private_keys.back().get_public_key();
         delegate_account.owner = delegate_public_key;
         delegate_account.is_delegate = true;
         
         config.names.push_back(delegate_account);
         config.balances.push_back( std::make_pair( pts_address(fc::ecc::public_key_data(delegate_account.owner)), BTS_BLOCKCHAIN_INITIAL_SHARES/BTS_BLOCKCHAIN_NUM_DELEGATES) );
      }

      fc::json::save_to_file( config, clienta_dir.path() / "genesis.json" );

      fc::json::save_to_file( config, clientb_dir.path() / "genesis.json" );

      auto sim_network = std::make_shared<bts::net::simulated_network>();

      clienta = std::make_shared<client>(sim_network);
      clienta->open( clienta_dir.path(), clienta_dir.path() / "genesis.json" );

      clienta->configure_from_command_line( 0, nullptr );
      clienta->start().wait();

      clientb = std::make_shared<client>(sim_network);
      clientb->open( clientb_dir.path(), clientb_dir.path() / "genesis.json" );
      clientb->configure_from_command_line( 0, nullptr );
      clientb->start().wait();
      //run_cmd(clientb, "help");
      run_cmd(clientb, "wallet_create walletb masterpassword 123456a123456789012345678901234567890");
      run_cmd(clienta, "wallet_create walleta masterpassword 123456ddddaxxx123456789012345678901234567890");

      int even = 0;
      for( auto key : delegate_private_keys )
      {
         if( even >= 30 )
         {
            if( (even++)%2 )
            {
          std::cerr << "client a key: "<< even<<" "<< clienta->execute_command_line( "wallet_import_private_key " + key_to_wif( key  ) ) << "\n";
            }
            else
            {
          std::cerr << "client b key: "<< even<<" "<< clientb->execute_command_line( "wallet_import_private_key " + key_to_wif( key  ) ) << "\n";
            }
            if( even >= 34 ) break;
         }
         else ++even;
      }
      
   }

   ~WFixture()
   {
   }

   std::shared_ptr<client> clienta;
   std::shared_ptr<client> clientb;
   fc::temp_directory clienta_dir;
   fc::temp_directory clientb_dir;
};

BOOST_FIXTURE_TEST_CASE( private_key_test, WFixture )
{
   run_cmd(clienta, "unlock 999999999 masterpassword");
   run_cmd(clienta, "scan 0 100");
   
   auto wallet = clienta->get_wallet();
   auto private_key = wallet->get_private_key(wallet->get_account("delegate31")->active_address());
   BOOST_CHECK(private_key == wallet->get_account_private_key("delegate31"));
}

BOOST_FIXTURE_TEST_CASE( account_address_test, WFixture )
{
   run_cmd(clienta, "unlock 999999999 masterpassword");
   run_cmd(clienta, "scan 0 100");
   
   run_cmd(clienta, "wallet_account_create test-1");

   auto wallet = clienta->get_wallet();
   
   auto account_pubkey = wallet->get_account_public_key("test-1");
   
   auto public_keys = wallet->get_public_keys_in_account("test-1");
   
   BOOST_CHECK( std::find( public_keys.begin(), public_keys.end(), account_pubkey) !=public_keys.end() );
}

/***
 *  This test case is designed to grow for ever and never shrink.  It is a complete scripted history
 *  of operations that should always work based upon a generated genesis condition.
 */
BOOST_FIXTURE_TEST_CASE( denny_test, WFixture )
{ try {
   infolog( "------------------  CLIENT A  -----------------------------------" );
   run_cmd(clienta, "wallet_list_receive_accounts");
   run_cmd(clienta, "wallet_account_balance");
   run_cmd(clienta, "unlock 999999999 masterpassword");
   run_cmd(clienta, "scan 0 100");
   run_cmd(clienta, "wallet_account_balance");
   run_cmd(clienta, "close");
   run_cmd(clienta, "open walleta");
   
   run_cmd(clienta, "unlock 99999999 masterpassword");
   run_cmd(clienta, "wallet_account_balance");
   run_cmd(clienta, "wallet_account_balance delegate31");
   
   infolog( "------------------  CLIENT B  -----------------------------------" );
   run_cmd(clientb, "wallet_list_receive_accounts");
   run_cmd(clientb, "wallet_account_balance");
   run_cmd(clientb, "unlock 999999999 masterpassword");
   
   run_cmd(clientb, "wallet_account_create b-account");
   run_cmd(clientb, "wallet_account_balance b-account");

} FC_LOG_AND_RETHROW() }

/***
 *  This test case is designed to grow for ever and never shrink.  It is a complete scripted history
 *  of operations that should always work based upon a generated genesis condition.
 */
BOOST_FIXTURE_TEST_CASE( wallet_account_name_test, WFixture )
{ try {
   infolog( "------------- start testing of account rename ---------------" );
   run_cmd(clienta, "unlock 999999999 masterpassword");
   run_cmd(clienta, "scan 0 100");
   run_cmd(clienta, "wallet_account_balance");
   run_cmd(clientb, "wallet_account_balance");

   run_cmd(clientb, "wallet_account_create b-account");
   run_cmd(clientb, "wallet_account_balance b-account");

   // register a new account called b-account
   run_cmd(clientb, "wallet_account_register b-account delegate30");

   // after registeration, and sync with new chain, b-account will be automaitically renmamed to delegate30
   run_cmd(clientb, "wallet_get_account b-account");
   produce_block( clienta );

   // should return null
   run_cmd(clientb, "blockchain_get_account_record b-account");

   run_cmd(clientb, "wallet_list_receive_accounts");

   run_cmd(clientb, "wallet_account_rename b-account b-new-account");

   produce_block(clientb);

   run_cmd(clientb, "blockchain_get_account_record b-new-account");

   // should not allowed rename, because this account name has already been registered on chain.
   run_cmd(clientb, "wallet_account_rename b-new-account another-account");

   run_cmd(clientb, "blockchain_get_account_record b-new-account");

   run_cmd(clientb, "blockchain_get_account_record another-account");

   run_cmd(clientb, "blockchain_get_account_record b-account");

   run_cmd(clientb, "wallet_list_receive_accounts");

   infolog("----------------- testing clienta registered a new account which clientb already have one on local, after sync the block, clientb should rename that to another name??");

   produce_block(clienta);

   run_cmd(clientb, "wallet_account_create x-account");

   produce_block(clientb);
   run_cmd(clienta, "wallet_account_create x-account");
   run_cmd(clienta, "wallet_account_register x-account delegate31");

   produce_block(clientb);

   run_cmd(clientb, "wallet_get_account x-account");
   run_cmd(clienta, "wallet_get_account x-account");
   run_cmd(clientb, "blockchain_get_account_record x-account");
} FC_LOG_AND_RETHROW() }
