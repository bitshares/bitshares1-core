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

#include <fc/io/raw.hpp>
#include <iostream>
#include <fstream>

using namespace bts::blockchain;

int main( int argc, char** argv )
{
   if( argc != 3 )
   {
      std::cout << "usage: "<<argv[0]<< " GENESIS_JSON  GENESIS_DAT\n";
      return -1;
   }
   auto config = fc::json::from_file( argv[1] ).as<genesis_block_config>();
   std::ofstream genesis_bin(argv[2]);
   fc::raw::pack( genesis_bin, config );

   return 0;
}
