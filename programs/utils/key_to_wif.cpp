#include <fc/crypto/elliptic.hpp>
#include <bts/utilities/key_conversion.hpp>
#include <fc/io/json.hpp>
#include <iostream>

int main( int argc, char** argv )
{
   if( argc < 2 ) 
      return -1;

   fc::ecc::private_key k = fc::json::from_string(argv[1]).as<fc::ecc::private_key>();
   std::cout << bts::utilities::key_to_wif(k);

   return 0;
}
