#include <bts/blockchain/address.hpp>
#include <bts/blockchain/genesis_config.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/pts_address.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/exception/exception.hpp>
#include <fc/log/logger.hpp>
#include <fc/io/json.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/filesystem.hpp>
#include <bts/utilities/key_conversion.hpp>

#include <iostream>

using namespace bts::blockchain;

struct founder
{
   std::string              keyhotee_id_utf8;
   int64_t                  balance;
   fc::ecc::public_key_data public_key;
};

FC_REFLECT( founder, (keyhotee_id_utf8)(balance)(public_key) )

void transform_name( std::string& name )
{
   for( char& c : name )
   {
      if( c == ' ' ) c = '-';
      if( c == '.' ) c = '-';
      if( c == '_' ) c = '-';
      if( c == '#' ) c = '-';
   }
}

int main( int argc, char** argv )
{
   genesis_block_config config;
   config.balances.resize( BTS_BLOCKCHAIN_NUM_DELEGATES );

   std::vector<fc::ecc::private_key> keys;
   keys.reserve( BTS_BLOCKCHAIN_NUM_DELEGATES );

   config.names.resize( BTS_BLOCKCHAIN_NUM_DELEGATES );
   for( unsigned i = 0; i < BTS_BLOCKCHAIN_NUM_DELEGATES; ++i )
   {
      keys.push_back( fc::ecc::private_key::generate() );
      config.names[i].name = "delegate"+fc::to_string(i);
      config.names[i].is_delegate = true;
      config.names[i].owner = keys[i].get_public_key().serialize();
      config.balances[i].first = pts_address( keys[i].get_public_key() );
      config.balances[i].second = 1000;
   }

   /*
   if( fc::exists( "founders.json" ) )
   {
      try {
      auto founders = fc::json::from_file( "founders.json" ).as<std::vector<founder>>();
      int64_t total_founder_balance = 0;
      for( auto f : founders )
         total_founder_balance += f.balance;

      double scale = 3623.188405796104 / total_founder_balance;

      for( auto f : founders )
      {
         config.names.resize( config.names.size() + 1 );
         config.names.back().name = fc::to_lower( f.keyhotee_id_utf8 );
         transform_name( config.names.back().name  );
         config.names.back().is_delegate = false;
         config.names.back().owner = f.public_key;

         config.balances.push_back( std::make_pair( pts_address( f.public_key ), (f.balance * scale) ) );
      }
      } catch ( const fc::exception& e )
      {
         elog( "${e}", ("e",e.to_detail_string() ) );
      }
   }
   */

   fc::json::save_to_file( config, fc::path("genesis.json"), true );
   std::vector<std::string> wif_keys;
   for( auto k : keys )
      wif_keys.push_back( bts::utilities::key_to_wif( k ) );
   fc::json::save_to_file( wif_keys, fc::path("genesis_private.json"), true );

   std::cout << "saving genesis.json\n";
   std::cout << "saving genesis_private.json\n";

   return 0;
}
