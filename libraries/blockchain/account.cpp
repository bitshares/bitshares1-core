#include <algorithm>

#include <fc/crypto/base58.hpp>
#include <fc/crypto/ripemd160.hpp>
#include <fc/crypto/sha512.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/exception/exception.hpp>

#include <bts/blockchain/config.hpp>
#include <bts/blockchain/account.hpp>

namespace bts {
  namespace blockchain {
   account::account(){}

   account::account( const std::string& base58str )
   { 
      FC_ASSERT( is_valid( base58str ) );
      std::vector<char> v = fc::from_base58( base58str.substr( strlen(BTS_ADDRESS_PREFIX) ) );
      memcpy( (char*)addr._hash, v.data(), std::min<size_t>( v.size()-4, sizeof(addr) ) );
   }

   /**
    *  Validates checksum and length of base58 account
    *
    *  @return true if successful, throws an exception with reason if invalid.
    */
   bool account::is_valid(const std::string& base58str )
   { try {
      FC_ASSERT( base58str.size() > strlen(BTS_ADDRESS_PREFIX) ); // could probably increase from 10 to some min length
      FC_ASSERT( base58str.substr( 0, strlen(BTS_ADDRESS_PREFIX) ) == BTS_ADDRESS_PREFIX );
      std::vector<char> v = fc::from_base58( base58str.substr( strlen(BTS_ADDRESS_PREFIX) ) );
      FC_ASSERT( v.size() > 4, "all accountes must have a 4 byte checksum" );
      auto checksum = fc::ripemd160::hash( v.data(), v.size() - 4 );
      FC_ASSERT( memcmp( v.data()+21, (char*)checksum._hash, 4 ) == 0, "account checksum mismatch" );
      return true;
   } FC_RETHROW_EXCEPTIONS( warn, "invalid account '${a}'", ("a", base58str) ) }

   account::account( const fc::sha512& condition_digest )
   {
       addr = fc::ripemd160::hash( condition_digest );
   }

   account::operator std::string()const
   {
        fc::array<char,25> bin_addr;
        memcpy( (char*)&bin_addr, (char*)&addr, sizeof(addr)+1 );
        bin_addr.data[20] = 88; // magic number 
        auto checksum = fc::ripemd160::hash( (char*)&bin_addr, sizeof(bin_addr)-4 );
        memcpy( ((char*)&bin_addr)+21, (char*)&checksum._hash[0], 4 );
        return BTS_ADDRESS_PREFIX + fc::to_base58( bin_addr.data, sizeof(bin_addr) );
   }

} } // namespace bts::blockchain


namespace fc 
{ 
   void to_variant( const bts::blockchain::account& var,  variant& vo )
   {
        vo = std::string(var);
   }
   void from_variant( const variant& var,  bts::blockchain::account& vo )
   {
        vo = bts::blockchain::account( var.as_string() );
   }
}
