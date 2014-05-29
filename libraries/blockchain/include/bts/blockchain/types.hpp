#pragma once
#include <fc/crypto/ripemd160.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/base58.hpp>
#include <fc/io/varint.hpp>
#include <fc/exception/exception.hpp>

#include <bts/blockchain/config.hpp>
#include <bts/blockchain/address.hpp>

namespace fc { 
   class path; 
   class microseconds;
   class time_point;
   class time_point_sec;
}

namespace bts { namespace blockchain {

   typedef fc::ripemd160                    block_id_type;
   typedef fc::ripemd160                    transaction_id_type;
   typedef fc::ripemd160                    public_key_hash_type;
   typedef fc::ripemd160                    secret_hash_type;
   typedef fc::sha256                       digest_type;
   typedef std::vector<transaction_id_type> transaction_ids;
   typedef fc::ecc::compact_signature       signature_type;
   typedef fc::ecc::private_key             private_key_type;
   typedef address                          balance_id_type;
   typedef fc::signed_int                   asset_id_type;
   /** @deprecated use account_id_type instead */
   typedef fc::signed_int                   name_id_type;
   typedef fc::signed_int                   account_id_type;
   typedef fc::signed_int                   proposal_id_type;
   typedef uint32_t                         tapos_type; 
   typedef int64_t                          share_type;
   typedef int64_t                          bip_type;

   using std::string;
   using std::function;
   using fc::variant;
   using fc::optional;
   using std::map;
   using std::unordered_map;
   using std::unordered_set;
   using std::vector;
   using fc::path;
   using fc::sha512;
   using fc::sha256;
   using std::unique_ptr;
   using std::shared_ptr;
   using fc::time_point_sec;
   using fc::time_point;
   using fc::microseconds;

   struct public_key_type 
   {
       fc::ecc::public_key_data key_data;

       public_key_type():key_data(){};

       public_key_type( const fc::ecc::public_key_data& data )
           :key_data( data ) {};

       public_key_type( const fc::ecc::public_key& pubkey )
           :key_data( pubkey ) {};

       public_key_type( const std::string& base58str )
       {
            FC_ASSERT( base58str.size() > strlen(BTS_ADDRESS_PREFIX) );
            FC_ASSERT( base58str.substr( 0, strlen(BTS_ADDRESS_PREFIX) ) == BTS_ADDRESS_PREFIX );
            std::vector<char> bin_dat = fc::from_base58( base58str.substr( strlen(BTS_ADDRESS_PREFIX ) ) );
            FC_ASSERT( bin_dat.size() == 37 );
            auto checksum = fc::ripemd160::hash( bin_dat.data(), bin_dat.size() - 4 );
            FC_ASSERT( 0 == memcmp( bin_dat.data()+33, (char*)checksum._hash, 4), "public_key_type checksum fail");
            memcpy( (char*)&key_data, (char*)&bin_dat, 33 ); 
       };

       operator fc::ecc::public_key_data() const
       {
          return key_data;    
       };
       operator fc::ecc::public_key() const
       {
          return fc::ecc::public_key( key_data );
       };

       operator std::string() const
       {
          FC_ASSERT(37 == key_data.size() + 4);
          fc::array<char, 37> bin_data;
          memcpy( (char*)&bin_data, (char*)&key_data, sizeof(key_data) );
          auto checksum = fc::ripemd160::hash( (char*)&key_data, sizeof(key_data) );
          memcpy( (char*)&bin_data + 33, (char*)checksum._hash, 4);
          return BTS_ADDRESS_PREFIX + fc::to_base58( bin_data.data, sizeof(bin_data) );
       }
       inline friend bool operator == ( const public_key_type& p1, const fc::ecc::public_key& p2)
       {
          return p1.key_data == p2.serialize();
       }

       inline friend bool operator == ( const public_key_type& p1, const public_key_type& p2)
       {
          return p1.key_data == p2.key_data;
       }
       inline friend bool operator != ( const public_key_type& p1, const public_key_type& p2)
       {
          return p1.key_data != p2.key_data;
       }
   };

   struct proposal_vote_id_type
   {
      proposal_vote_id_type( proposal_id_type proposal_id_arg = 0, name_id_type delegate_id_arg = 0 )
         :proposal_id(proposal_id_arg),delegate_id(delegate_id_arg){}

      proposal_id_type proposal_id;
      name_id_type     delegate_id;

      proposal_vote_id_type& operator=( const proposal_vote_id_type& other )
      {
         proposal_id = other.proposal_id;
         delegate_id = other.delegate_id;
         return *this;
      }
      friend bool operator <  ( const proposal_vote_id_type& a, const proposal_vote_id_type& b )
      {
         if( a.proposal_id == b.proposal_id )
            return a.delegate_id < b.delegate_id;
         return a.proposal_id < b.proposal_id;
      }
      friend bool operator ==  ( const proposal_vote_id_type& a, const proposal_vote_id_type& b )
      {
         return a.proposal_id == b.proposal_id  && a.delegate_id == b.delegate_id;
      }
   };

   #define BASE_ASSET_ID  (asset_id_type())

} } // bts::blockchain


namespace fc
{
    void to_variant( const bts::blockchain::public_key_type& var,  fc::variant& vo );
    void from_variant( const fc::variant& var,  bts::blockchain::public_key_type& vo );
}


#include <fc/reflect/reflect.hpp>
FC_REFLECT( bts::blockchain::proposal_vote_id_type, (proposal_id)(delegate_id) )
FC_REFLECT( bts::blockchain::public_key_type, (key_data) )
