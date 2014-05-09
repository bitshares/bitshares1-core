#pragma once
#include <fc/array.hpp>
#include <fc/crypto/ripemd160.hpp>
#include <string>

namespace fc { namespace ecc { 
    class public_key; 
    typedef fc::array<char,33>  public_key_data; 
} } // fc::ecc 

namespace bts { namespace blockchain {

   /**
    *  @brief a 160 bit hash of a public key
    *
    *  An account can be converted to or from a base58 string with 32 bit checksum.
    *
    *  An account is calculated as ripemd160( sha512( compressed_ecc_public_key ) ) 
    *
    *  When converted to a string, checksum calculated as the first 4 bytes ripemd160( account ) is
    *  appended to the binary account before converting to base58.
    */
   class account
   {
      public:
       account(); ///< constructs empty / null account
       account( const std::string& base58str );   ///< converts to binary, validates checksum
       account( const fc::sha512& condition_digest ); ///< converts to binary

       static bool is_valid(const std::string& base58str );
       operator    std::string()const; ///< converts to base58 + checksum

       fc::ripemd160      addr;
   };
   inline bool operator == ( const account& a, const account& b ) { return a.addr == b.addr; }
   inline bool operator != ( const account& a, const account& b ) { return a.addr != b.addr; }
   inline bool operator <  ( const account& a, const account& b ) { return a.addr <  b.addr; }

} } // namespace bts::blockchain


namespace fc 
{ 
   void to_variant( const bts::blockchain::account& var,  fc::variant& vo );
   void from_variant( const fc::variant& var,  bts::blockchain::account& vo );
}

namespace std
{
   template<>
   struct hash<bts::blockchain::account> 
   {
       public:
         size_t operator()(const bts::blockchain::account &a) const 
         {
            return (uint64_t(a.addr._hash[0])<<32) | uint64_t( a.addr._hash[0] );
         }
   };
}

#include <fc/reflect/reflect.hpp>
FC_REFLECT( bts::blockchain::account, (addr) )
