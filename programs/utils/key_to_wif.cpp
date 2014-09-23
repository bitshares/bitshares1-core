#include <fc/crypto/elliptic.hpp>
#include <bts/utilities/key_conversion.hpp>
#include <fc/io/json.hpp>
#include <iostream>
#include <fc/exception/exception.hpp>

int main( int argc, char** argv )
{
   if( argc < 2 ) 
      return -1;

   try{
     fc::ecc::private_key k = fc::ecc::private_key::regenerate(fc::sha256(std::string(argv[1])));
     std::cout << bts::utilities::key_to_wif(k) << "\n";
   } catch (fc::exception e) {
     std::cout << e.to_detail_string();
   }

   return 0;
}
