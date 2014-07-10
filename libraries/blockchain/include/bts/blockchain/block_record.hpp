#pragma once

#include <bts/blockchain/block.hpp>
#include <bts/blockchain/transaction_evaluation_state.hpp>

namespace bts { namespace blockchain {

   struct block_record : public bts::blockchain::digest_block
   {
      block_record():block_size(0),total_fees(0),latency(0),processing_time(0){}
      block_record( const digest_block& b, const fc::ripemd160& r, uint64_t s, const fc::microseconds& l )
      :digest_block(b),random_seed(r),block_size(s),total_fees(0),latency(l),processing_time(0){}

      fc::ripemd160     random_seed;
      uint64_t          block_size; /* Bytes */
      share_type        total_fees;
      fc::microseconds  latency; /* Time between block timestamp and first push_block */
      fc::microseconds  processing_time; /* Time taken for most recent push_block to run */
   };
   typedef optional<block_record> oblock_record;

   struct transaction_record : public transaction_evaluation_state
   {
      transaction_record(){}

      transaction_record( const transaction_location& loc, 
                          const transaction_evaluation_state& s )
      :transaction_evaluation_state(s),chain_location(loc){}

      transaction_location chain_location;
   };
   typedef optional<transaction_record> otransaction_record;

   struct slot_record
   {
      slot_record( const time_point_sec& t, const account_id_type& d, bool p = false, const block_id_type& b = block_id_type() )
      :start_time(t),block_producer_id(d),block_produced(p),block_id(b){}

      slot_record()
      :block_produced(false){}

      time_point_sec  start_time;
      account_id_type block_producer_id;
      bool            block_produced;
      block_id_type   block_id;
   };
   typedef fc::optional<slot_record> oslot_record;

} } // bts::blockchain

FC_REFLECT_DERIVED( bts::blockchain::block_record, 
                    (bts::blockchain::digest_block), 
                    (random_seed)
                    (block_size)
                    (total_fees)
                    (latency)
                    (processing_time) )

FC_REFLECT_DERIVED( bts::blockchain::transaction_record, 
                    (bts::blockchain::transaction_evaluation_state), 
                    (chain_location) );

FC_REFLECT( bts::blockchain::slot_record, (start_time)(block_producer_id)(block_produced)(block_id) )
