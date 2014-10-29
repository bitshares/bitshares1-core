#pragma once

#include <bts/blockchain/address.hpp>

#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/sha1.hpp>
#include <fc/reflect/reflect.hpp>

namespace bts { namespace blockchain {

    /**
     *  Given an extended public key you can calculate the public key of all
     *  children keys, but not the corresponding private keys.
     *
     *  @note this only works for extended private keys that use public derivation
     */
    class extended_public_key
    {
       public:
          extended_public_key();
          virtual ~extended_public_key();

          extended_public_key( const fc::ecc::public_key& key, const fc::sha256& code = fc::sha256() );

          extended_public_key child( uint32_t c )const;
          extended_public_key child( const fc::sha256& secret )const;

          fc::ecc::public_key get_pub_key()const { return pub_key; }
          operator address()const { return address(pub_key); }

          operator fc::ecc::public_key()const { return pub_key; }

          fc::ecc::public_key pub_key;
          fc::sha256          chain_code;

          inline friend bool operator < ( const extended_public_key& a, const extended_public_key& b )
          {
             if( a.chain_code < b.chain_code   ) return true;
             if( a.chain_code > b.chain_code   ) return false;
             if( a.pub_key.serialize() < b.pub_key.serialize() ) return true;
             return false;
          }
          inline friend bool operator == ( const extended_public_key& a, const extended_public_key& b )
          {
             if( a.chain_code != b.chain_code   ) return false;
             if( a.pub_key.serialize() != b.pub_key.serialize() ) return false;
             return true;
          }
          inline friend bool operator != ( const extended_public_key& a, const extended_public_key& b )
          {
              return !(a==b);
          }
    };

    class extended_private_key
    {
       public:
          enum derivation_type
          {
             public_derivation = 0,
             private_derivation = 1
          };
          extended_private_key( const fc::sha512& seed );
          extended_private_key( const fc::sha256& key, const fc::sha256& chain_code );
          extended_private_key( const fc::ecc::private_key& k ):priv_key(k.get_secret()){}
          extended_private_key();

          /** @param pub_derivation - if true, then extended_public_key can be used
           *      to calculate child keys, otherwise the extended_private_key is
           *      required to calculate all children.
           */
          extended_private_key child( uint32_t c, 
                                      derivation_type derivation = private_derivation )const;

          extended_private_key child( const fc::sha256& secret, 
                                      derivation_type derivation = private_derivation )const;

          operator fc::ecc::private_key()const;
          fc::ecc::public_key get_public_key()const;

          operator extended_public_key()const
          {
             return extended_public_key( fc::ecc::private_key::regenerate(priv_key).get_public_key(), 
                                         chain_code);
          }
          
          bool operator==( const extended_private_key& k ) const
          {
		      return (this->priv_key == k.priv_key) &&
			         (this->chain_code == k.chain_code);
		  }

          fc::ecc::private_key_secret  priv_key;
          fc::sha256                   chain_code;
    };

   /**
    *  @brief encapsulates an encoded, checksumed public key in
    *  binary form. It can be converted to base58 for display
    *  or input purposes and can also be constructed from an ecc
    *  public key.
    *
    *  An valid extended_address is 20 bytes with the following form:
    *
    *  First 3-bits are 0, followed by bits to 3-127 of sha256(pub_key), followed
    *  a 32 bit checksum calculated as the first 32 bits of the sha256 of
    *  the first 128 bits of the extended_address.
    *
    *  The result of this is an extended_address that can be 'verified' by
    *  looking at the first character (base58) and has a built-in
    *  checksum to prevent mixing up characters.
    *
    *  It is stored as 20 bytes.
    */
   struct extended_address
   {
       extended_address(); ///< constructs empty / null extended_address
       extended_address( const std::string& base58str );   ///< converts to binary, validates checksum
       extended_address( const extended_public_key& pub );

       bool is_valid()const;

       operator std::string()const; ///< converts to base58 + checksum
       operator extended_public_key()const; ///< converts to base58 + checksum

       extended_public_key addr;      ///< binary representation of extended_address
   };

   inline bool operator == ( const extended_address& a, const extended_address& b ) { return a.addr == b.addr; }
   inline bool operator != ( const extended_address& a, const extended_address& b ) { return a.addr != b.addr; }
   inline bool operator <  ( const extended_address& a, const extended_address& b ) { return a.addr <  b.addr; }

} } // namespace bts::blockchain

FC_REFLECT( bts::blockchain::extended_public_key, (pub_key)(chain_code) )
FC_REFLECT( bts::blockchain::extended_private_key, (priv_key)(chain_code) )
FC_REFLECT( bts::blockchain::extended_address, (addr) )

namespace fc
{
   void to_variant( const bts::blockchain::extended_address& var,  fc::variant& vo );
   void from_variant( const fc::variant& var,  bts::blockchain::extended_address& vo );
}

namespace std
{
   template<>
   struct hash<bts::blockchain::extended_address>
   {
       public:
         size_t operator()(const bts::blockchain::extended_address &a) const
         {
            size_t s;
            fc::sha1::encoder enc;
            fc::raw::pack( enc, a );
            auto r = enc.result();

            memcpy( (char*)&s, (char*)&r, sizeof(s) );
            return s;
         }
   };
}
