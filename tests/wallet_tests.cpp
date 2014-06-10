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
#include <fc/io/json.hpp>
#include <fc/thread/thread.hpp>
#include <iostream>
#include <bts/utilities/key_conversion.hpp>

#include <fc/network/http/connection.hpp>

using namespace bts::blockchain;
using namespace bts::wallet;
using namespace bts::utilities;
using namespace bts::client;
using namespace bts::cli;

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

BOOST_AUTO_TEST_CASE( wif_format_test )
{
  try {
   auto priv_key = fc::variant( "0c28fca386c7a227600b2fe50b7cae11ec86d3bf1fbe471be89827e19d72aa1d" ).as<fc::ecc::private_key>();
   FC_ASSERT( bts::utilities::key_to_wif(priv_key) == "5HueCGU8rMjxEXxiPuD5BDku4MkFqeZyd4dZ1jvhTVqvbTLvyTJ" );
   FC_ASSERT( bts::utilities::wif_to_key( "5HueCGU8rMjxEXxiPuD5BDku4MkFqeZyd4dZ1jvhTVqvbTLvyTJ" ).valid() );
   wif_to_key( key_to_wif( fc::ecc::private_key::generate() ) );
  } FC_LOG_AND_RETHROW() 
}

template<typename T>
void produce_block( T my_client )
{
      auto head_num = my_client->get_chain()->get_head_block_num();
      auto next_block_production_time = my_client->get_wallet()->next_block_production_time();
      bts::blockchain::advance_time( (int32_t)((next_block_production_time - bts::blockchain::now()).count()/1000000) );
      auto b = my_client->get_chain()->generate_block(next_block_production_time);
      my_client->get_wallet()->sign_block( b );
      my_client->get_node()->broadcast( bts::client::block_message( b ) );
      FC_ASSERT( head_num+1 == my_client->get_chain()->get_head_block_num() );
}


/***
 *  This test case is designed to grow for ever and never shrink.  It is a complete scripted history
 *  of operations that should always work based upon a generated genesis condition.
 */
BOOST_AUTO_TEST_CASE( master_test )
{ try {
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
      delegate_account.owner = delegate_private_keys.back().get_public_key();
      delegate_account.is_delegate = true;

      config.names.push_back(delegate_account);
      config.balances.push_back( std::make_pair( pts_address(fc::ecc::public_key_data(delegate_account.owner)), BTS_BLOCKCHAIN_INITIAL_SHARES/BTS_BLOCKCHAIN_NUM_DELEGATES) );
   }

   fc::temp_directory clienta_dir;
   fc::json::save_to_file( config, clienta_dir.path() / "genesis.json" );

   fc::temp_directory clientb_dir;
   fc::json::save_to_file( config, clientb_dir.path() / "genesis.json" );

   auto sim_network = std::make_shared<bts::net::simulated_network>();

   auto clienta = std::make_shared<client>(sim_network);
   clienta->open( clienta_dir.path(), clienta_dir.path() / "genesis.json" );

   auto clientb = std::make_shared<client>(sim_network);
   clientb->open( clientb_dir.path(), clientb_dir.path() / "genesis.json" );

   std::cerr << clientb->execute_command_line( "help" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_create walletb masterpassword" ) << "\n";
   std::cerr << clienta->execute_command_line( "wallet_create walleta masterpassword" ) << "\n";

   int even = 0;
   for( auto key : delegate_private_keys )
   {
      if( (even++)%2 )
         std::cerr << clienta->execute_command_line( "wallet_import_private_key " + key_to_wif( key  ) ) << "\n";
      else
         std::cerr << clientb->execute_command_line( "wallet_import_private_key " + key_to_wif( key  ) ) << "\n";
      if( even >= 4 ) break;
   }
   wlog( "------------------  CLIENT A  -----------------------------------" );
   std::cerr << clienta->execute_command_line( "wallet_list_receive_accounts" ) << "\n";
   std::cerr << clienta->execute_command_line( "wallet_account_balance" ) << "\n";
   std::cerr << clienta->execute_command_line( "wallet_account_balance delegate1" ) << "\n";
   std::cerr << clienta->execute_command_line( "unlock 999999999 masterpassword" ) << "\n";

   wlog( "------------------  CLIENT B  -----------------------------------" );
   std::cerr << clientb->execute_command_line( "wallet_list_receive_accounts" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_account_balance" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_account_balance delegate0" ) << "\n";
   std::cerr << clientb->execute_command_line( "unlock 999999999 masterpassword" ) << "\n";

   std::cerr << clientb->execute_command_line( "wallet_account_create b-account" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_account_balance b-account" ) << "\n";

   std::cerr << clientb->execute_command_line( "wallet_account_register b-account delegate0" );
   wlog( "------------------  CLIENT A  -----------------------------------" );
   produce_block( clienta );
   wlog( "------------------  CLIENT B  -----------------------------------" );
   std::cerr << clientb->execute_command_line( "wallet_list_receive_accounts" );
   std::cerr << clientb->execute_command_line( "wallet_account_update_registration b-account delegate0 { \"ip\":\"localhost\"} true" );
   wlog( "------------------  CLIENT A  -----------------------------------" );
   produce_block( clienta );
   wlog( "------------------  CLIENT B  -----------------------------------" );
   std::cerr << clientb->execute_command_line( "wallet_list_receive_accounts" );
   wlog( "------------------  CLIENT A  -----------------------------------" );
   std::cerr << clienta->execute_command_line( "wallet_transfer 33 XTS delegate1 b-account first-memo" ) << "\n";
   std::cerr << clienta->execute_command_line( "wallet_account_transaction_history delegate1" ) << "\n";
   std::cerr << clienta->execute_command_line( "wallet_account_transaction_history b-account" ) << "\n";
   wlog( "------------------  CLIENT B  -----------------------------------" );
   std::cerr << clientb->execute_command_line( "wallet_account_transaction_history b-account" ) << "\n";
   produce_block( clientb );
   wlog( "------------------  CLIENT A  -----------------------------------" );
   std::cerr << clienta->execute_command_line( "wallet_account_transaction_history delegate1" ) << "\n";
   std::cerr << clienta->execute_command_line( "wallet_account_transaction_history b-account" ) << "\n";
   wlog( "------------------  CLIENT B  -----------------------------------" );
   std::cerr << clientb->execute_command_line( "wallet_account_transaction_history b-account" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_create_account c-account" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_transfer 10 XTS b-account c-account to-me" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_account_transaction_history b-account" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_account_transaction_history c-account" ) << "\n";
   produce_block( clientb );
   std::cerr << clientb->execute_command_line( "wallet_account_transaction_history c-account" ) << "\n";
   std::cerr << clientb->execute_command_line( "blockchain_list_delegates" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_set_delegate_trust_level b-account 1" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_list_receive_accounts" ) << "\n";
   std::cerr << clientb->execute_command_line( "balance" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_transfer 100000 XTS delegate2 c-account to-me" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_transfer 100000 XTS delegate0 c-account to-me" ) << "\n";
   produce_block( clientb );
   std::cerr << clientb->execute_command_line( "balance" ) << "\n";
   std::cerr << clientb->execute_command_line( "wallet_account_transaction_history c-account" ) << "\n";
   std::cerr << clientb->execute_command_line( "blockchain_list_delegates" ) << "\n";

   //std::cerr << clientb->execute_command_line( "wallet_list_receive_accounts" ) << "\n";
   //std::cerr << clientb->execute_command_line( "wallet_account_balance" ) << "\n";

} FC_LOG_AND_RETHROW() }


#include <fstream>

string extract_commands_from_log_stream(std::istream& log_stream)
{
  string command_list;
  string line;
  while (std::getline(log_stream,line))
  {
    //if line begins with a prompt, add to input buffer
    size_t prompt_position = line.find(CLI_PROMPT_SUFFIX);
    if (prompt_position != string::npos )
    { 
      size_t command_start_position = prompt_position + strlen(CLI_PROMPT_SUFFIX);
      command_list += line.substr(command_start_position);
      command_list += "\n";
    }
  }
  return command_list;
}

string extract_commands_from_log_file(fc::path test_file)
{
  std::ifstream test_input(test_file.string());
  return extract_commands_from_log_stream(test_input);
}


