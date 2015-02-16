#pragma once

#include <bts/blockchain/types.hpp>

namespace bts { namespace blockchain {

struct vote_del
{
   share_type        votes = 0;
   account_id_type   delegate_id = 0;

   vote_del( share_type v = 0, account_id_type del = 0 )
   :votes(v),delegate_id(del){}

   friend bool operator == ( const vote_del& a, const vote_del& b )
   {
       return std::tie( a.votes, a.delegate_id ) == std::tie( b.votes, b.delegate_id );
   }

   friend bool operator < ( const vote_del& a, const vote_del& b )
   {
       if( a.votes != b.votes ) return a.votes > b.votes; /* Reverse so maps sort in descending order */
       return a.delegate_id < b.delegate_id; /* Lowest id wins in ties */
   }
};

struct delegate_record;
typedef fc::optional<delegate_record> odelegate_record;

class chain_interface;
struct delegate_record
{
   delegate_id_type             id = 0;

   uint8_t                      pay_rate = 0;
   bool                         retracted = false;

   share_type                   votes_for = 0;

   uint32_t                     last_block_num_produced = 0;
   optional<secret_hash_type>   next_secret_hash;

   share_type                   pay_balance = 0;
   share_type                   total_paid = 0;
   share_type                   total_burned = 0;

   uint32_t                     blocks_produced = 0;
   uint32_t                     blocks_missed = 0;

   void sanity_check( const chain_interface& )const;
   static odelegate_record lookup( const chain_interface&, const delegate_id_type );
   static void store( chain_interface&, const delegate_id_type, const delegate_record& );
   static void remove( chain_interface&, const delegate_id_type );
};

class delegate_db_interface
{
   friend struct delegate_record;

   virtual odelegate_record delegate_lookup_by_id( const delegate_id_type )const = 0;

   virtual void delegate_insert_into_id_map( const delegate_id_type, const delegate_record& ) = 0;
   virtual void delegate_insert_into_vote_set( const vote_del ) = 0;

   virtual void delegate_erase_from_id_map( const delegate_id_type ) = 0;
   virtual void delegate_erase_from_vote_set( const vote_del ) = 0;
};

} } // bts::blockchain

FC_REFLECT( bts::blockchain::vote_del,
            (votes)
            (delegate_id)
            )
FC_REFLECT( bts::blockchain::delegate_record,
            (id)
            (pay_rate)
            (retracted)
            (votes_for)
            (last_block_num_produced)
            (next_secret_hash)
            (pay_balance)
            (total_paid)
            (total_burned)
            (blocks_produced)
            (blocks_missed)
            )
