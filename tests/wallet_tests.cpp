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


