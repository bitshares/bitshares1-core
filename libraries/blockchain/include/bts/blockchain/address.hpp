#pragma once
#include <fc/array.hpp>
#include <string>

namespace fc { namespace ecc { class public_key; } }

namespace bts { namespace blockchain {

   /**
    *  @brief encapsulates an encoded, checksumed public key in
    *  binary form.   It can be converted to base58 for display
    *  or input purposes and can also be constructed from an ecc 
    *  public key.
    *
    *  An valid address is 20 bytes with the following form:
    *
    *  First 3-bits are 0, followed by bits to 3-127 of sha256(pub_key), followed 
    *  a 32 bit checksum calculated as the first 32 bits of the sha256 of
    *  the first 128 bits of the address.
    *
    *  The result of this is an address that can be 'verified' by
    *  looking at the first character (base58) and has a built-in
    *  checksum to prevent mixing up characters.
    *
    *  It is stored as 20 bytes.
    */
   class address
   {
      public:
       address(); ///< constructs empty / null address
       address( const std::string& base58str );   ///< converts to binary, validates checksum
       address( const fc::ecc::public_key& pub ); ///< converts to binary

       bool is_valid()const;

       operator std::string()const; ///< converts to base58 + checksum

       fc::array<char,20> addr;      ///< binary representation of address
   };
   inline bool operator == ( const address& a, const address& b ) { return a.addr == b.addr; }
   inline bool operator != ( const address& a, const address& b ) { return a.addr != b.addr; }
   inline bool operator <  ( const address& a, const address& b ) { return a.addr <  b.addr; }

} } // namespace bts::blockchain


namespace fc 
{ 
   void to_variant( const bts::blockchain::address& var,  fc::variant& vo );
   void from_variant( const fc::variant& var,  bts::blockchain::address& vo );
}

namespace std
{
   template<>
   struct hash<bts::blockchain::address> 
   {
       public:
         size_t operator()(const bts::blockchain::address &a) const 
         {
            size_t s;
            memcpy( (char*)&s, &a.addr.data[sizeof(a)-sizeof(s)], sizeof(s) );
            return s;
         }
   };
}

#include <fc/reflect/reflect.hpp>
FC_REFLECT( bts::blockchain::address, (addr) )
