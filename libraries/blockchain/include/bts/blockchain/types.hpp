#pragma once
#include <fc/crypto/ripemd160.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/base58.hpp>
#include <fc/io/varint.hpp>
#include <fc/exception/exception.hpp>

#include <bts/blockchain/config.hpp>
#include <bts/blockchain/address.hpp>

#include <functional>

namespace fc { 
   class path; 
   class microseconds;
   class time_point;
   class time_point_sec;
}

namespace bts { namespace blockchain {

   typedef fc::ripemd160                      block_id_type;
   typedef fc::ripemd160                      transaction_id_type;
   typedef fc::ripemd160                      public_key_hash_type;
   typedef fc::ripemd160                      secret_hash_type;
   typedef fc::sha256                         digest_type;
   typedef std::vector<transaction_id_type>   transaction_ids;
   typedef fc::ecc::compact_signature         signature_type;
   typedef fc::ecc::private_key               private_key_type;
   typedef address                            balance_id_type;
   typedef fc::signed_int                     asset_id_type;
   /** @deprecated use account_id_type instead */
   typedef fc::signed_int                     name_id_type;
   typedef fc::signed_int                     account_id_type;
   typedef fc::signed_int                     proposal_id_type;
   typedef uint32_t                           tapos_type; 
   typedef int64_t                            share_type;
   typedef int64_t                            bip_type;
   typedef fc::optional<fc::ecc::private_key> oprivate_key;

   using std::string;
   using std::function;
   using fc::variant;
   using fc::variant_object;
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
       struct binary_key
       {
          binary_key():check(0){}
          uint32_t                 check;
          fc::ecc::public_key_data data;
       };

       fc::ecc::public_key_data key_data;
 
       public_key_type();
       public_key_type( const fc::ecc::public_key_data& data );
       public_key_type( const fc::ecc::public_key& pubkey );
       explicit public_key_type( const std::string& base58str );
       operator fc::ecc::public_key_data() const;
       operator fc::ecc::public_key() const;
       explicit operator std::string() const;
       friend bool operator == ( const public_key_type& p1, const fc::ecc::public_key& p2);
       friend bool operator == ( const public_key_type& p1, const public_key_type& p2);
       friend bool operator != ( const public_key_type& p1, const public_key_type& p2);
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

   struct blockchain_security_state {
       enum alert_level_enum {
           green = 0,
           yellow = 1,
           red = 2
       };
       alert_level_enum    alert_level;
       uint32_t            estimated_confirmation_seconds;
       double              participation_rate;
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
FC_REFLECT( bts::blockchain::public_key_type::binary_key, (data)(check) );
FC_REFLECT_ENUM( bts::blockchain::blockchain_security_state::alert_level_enum, (green)(yellow)(red) );
FC_REFLECT( bts::blockchain::blockchain_security_state, (alert_level)(estimated_confirmation_seconds)(participation_rate) )
