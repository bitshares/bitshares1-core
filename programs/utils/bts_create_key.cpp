#include <bts/blockchain/address.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/io/json.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/filesystem.hpp>

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
   else if (argc == 2)
   {
     std::cout << "writing new private key to JSON file " << argv[1] << "\n";
     fc::ecc::private_key key(fc::ecc::private_key::generate());
     fc::json::save_to_file(key, fc::path(argv[1]));

     std::cout << "bts address: "
       << std::string(bts::blockchain::address(key.get_public_key())) << "\n";

     return 0;
   }
}
