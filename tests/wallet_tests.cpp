#define BOOST_TEST_MODULE BlockchainTests2
#include <boost/test/unit_test.hpp>
#include <bts/blockchain/chain_database.hpp>
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
   config.precision         = BTS_BLOCKCHAIN_PRECISION
   config.timestamp         = bts::blockchain::now();
   config.base_symbol       = BTS_BLOCKCHAIN_SYMBOL;
   config.base_name         = BTS_BLOCKCHAIN_NAME;
   config.base_description  = BTS_BLOCKCHAIN_DESCRIPTION;
   config.supply            = BTS_BLOCKCHAIN_INITIAL_SHARES;

   for( uint32_t i = 0; i < BTS_BLOCKCHAIN_NUM_DELEGATES; ++i )
   {
      name_config delegate_name;
      delegate_account.name = "delegate" + fc::to_string(i);
      delegate_private_keys.push_back( fc::ecc::private_key::generate() );
      delegate_account.owner = delegate_private_keys.back().get_public_key();
      delegate_account.is_delegate = true;

      config.names.push_back(delegate_account);
      balances.push_back( std::make_pair( pts_address( delegate_account.owner, BTS_BLOCKCHAIN_INITIAL_SHARES/BTS_BLOCKCHAIN_NUM_DELEGATES) ) );
   }

   fc::temp_directory clienta_dir;
   fc::json::save_to_file( config, clienta_dir / "genesis.json" );

   fc::temp_directory clientb_dir;
   fc::json::save_to_file( config, clientb_dir / "genesis.json" );

   auto sim_network = std::make_shared<simulated_network>();

   auto clienta = std::make_shared<client>(sim_network);
   clienta->open( clienta_dir.path(), clienta_dir / "genesis.json" );

   auto clientb = std::make_shared<client>(sim_network);
   clientb->open( clienta_dir.path(), clientb_dir / "genesis.json" );


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


