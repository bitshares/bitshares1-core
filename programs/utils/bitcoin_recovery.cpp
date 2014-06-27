#include "../../libraries/bitcoin/include/bts/bitcoin/bitcoin.hpp"
#include <bts/blockchain/pts_address.hpp>
#include <iostream>
#include <iostream>
#include <fc/exception/exception.hpp>

using namespace bts::blockchain;

int main( int argc, char** argv )
{
   try {
      std::cout << "password: ";
      std::string line;
      std::getline( std::cin, line );
      auto keys = bts::bitcoin::import_bitcoin_wallet( fc::path(argv[1]), line );
      for( auto key : keys )
      {
         std::cout << "importing " << std::string( pts_address( key.get_public_key(), true, 56 ) ) << "\n";
         std::cout << "importing " << std::string( pts_address( key.get_public_key(), false, 56 ) ) << "\n";
         std::cout << "importing " << std::string( pts_address( key.get_public_key(), true, 0 ) ) << "\n";
         std::cout << "importing " << std::string( pts_address( key.get_public_key(), false, 0 ) ) << "\n";
      }
   } catch ( const fc::exception& e )
   {
      elog( "${e}", ("e",e.to_detail_string() ) );
   }
   return 0;
}
