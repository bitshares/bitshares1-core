#pragma once
#include <fc/crypto/ripemd160.hpp>
#include <fc/crypto/elliptic.hpp>
#include <bts/blockchain/address.hpp>
#include <fc/io/varint.hpp>

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
   typedef fc::signed_int                   name_id_type;
   typedef fc::signed_int                   proposal_id_type;
   typedef uint32_t                         tapos_type; 
   typedef int64_t                          share_type;
   typedef int64_t                          bip_type;

   struct public_key_type 
   {
       fc::ecc::public_key_data key_data;

       public_key_type():key_data(){};
       public_key_type( fc::ecc::public_key_data data )
           :key_data( data ) {};
       public_key_type( fc::ecc::public_key pubkey )
           :key_data( pubkey ) {};
       operator fc::ecc::public_key_data() const
       {
          return key_data;    
       };
       operator fc::ecc::public_key() const
       {
          return fc::ecc::public_key( key_data );
       };

       operator bts::blockchain::address() const
       {
          return bts::blockchain::address( key_data );
       }
//     operator std::string() const;

       inline friend bool operator ==(public_key_type p1, public_key_type p2)
       {
          return p1.key_data == p2.key_data;
       }
       inline friend bool operator !=(public_key_type p1, public_key_type p2)
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
