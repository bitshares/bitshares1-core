#include <bts/blockchain/address.hpp>
#include <bts/blockchain/pts_address.hpp>
#include <bts/blockchain/types.hpp>
#include <bts/utilities/key_conversion.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/io/json.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/filesystem.hpp>

#include <iostream>

using namespace bts::blockchain;

int main( int argc, char** argv )
{
   if( argc == 1 )
   {
      auto key = fc::ecc::private_key::generate();
      std::cout << "public key: " 
                << std::string( public_key_type(key.get_public_key()) ) <<"\n";
      std::cout << "private key: " 
                << std::string( key.get_secret() ) <<"\n";

      std::cout << "private key WIF format: " <<
          std::string(bts::utilities::key_to_wif(key)) << "\n";


      std::cout << "bts address: " 
                << std::string( bts::blockchain::address( key.get_public_key() ) ) <<"\n";

      std::cout << "pts address: " 
                << std::string( bts::blockchain::pts_address( key.get_public_key() ) ) <<"\n";
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
