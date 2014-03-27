#include <bts/blockchain/address.hpp>
#include <fc/crypto/elliptic.hpp>

#include <iostream>

int main( int argc, char** argv )
{
   if( argc == 1 )
   {
      auto key = fc::ecc::private_key::generate();
      std::cout << "private key: " 
                << std::string( key.get_secret() ) <<"\n";

      std::cout << "bts address: " 
                << std::string( bts::blockchain::address( key.get_public_key() ) ) <<"\n";
      return 0;
   }
}
