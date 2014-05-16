#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/crypto/base58.hpp>
#include <fc/io/json.hpp>
#include <iostream>

int main( int argc, char** argv )
{
   if( argc < 2 ) 
      return -1;

   fc::ecc::private_key k = fc::json::from_string(argv[1]).as<fc::ecc::private_key>();
   auto secret = k.get_secret();

   std::vector<char> data;
   data.push_back( (char)0x80 );
   data.resize( 1 + sizeof(secret) );
   memcpy( &data[1], (char*)&secret, sizeof(secret) );
   auto digest = fc::sha256::hash( data.data(), data.size() );

   data.resize( data.size() + 4 );
   memcpy( data.data() + data.size() -4, (char*)&digest, 4 );

  
   std::cout <<  fc::to_base58( data.data(), data.size() ) << "\n";

   return 0;
}
