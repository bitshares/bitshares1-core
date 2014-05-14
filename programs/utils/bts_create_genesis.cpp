#include <bts/blockchain/address.hpp>
#include <bts/blockchain/genesis_config.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/pts_address.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/io/json.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/filesystem.hpp>

#include <iostream>

using namespace bts::blockchain;

int main( int argc, char** argv )
{
   genesis_block_config config;
   config.balances.resize( BTS_BLOCKCHAIN_NUM_DELEGATES );

   std::vector<fc::ecc::private_key> keys;
   keys.reserve( BTS_BLOCKCHAIN_NUM_DELEGATES );

   config.names.resize( BTS_BLOCKCHAIN_NUM_DELEGATES );
   for( uint64_t i = 0; i < BTS_BLOCKCHAIN_NUM_DELEGATES; ++i )
   {
      keys.push_back( fc::ecc::private_key::generate() );
      config.names[i].name = "delegate-"+fc::to_string(i);
      config.names[i].is_delegate = true;
      config.names[i].owner = keys[i].get_public_key().serialize();
      config.balances[i].first = pts_address( keys[i].get_public_key() );
      config.balances[i].second = 1000;
   }

   fc::json::save_to_file( config, fc::path("genesis.json"), true );
   fc::json::save_to_file( keys, fc::path("genesis_private.json"), true );

   std::cout << "saving genesis.json\n";
   std::cout << "saving genesis_private.json\n";

   return 0;
}
