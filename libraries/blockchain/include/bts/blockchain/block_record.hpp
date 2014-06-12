#pragma once
#include <bts/blockchain/block.hpp>
#include <bts/blockchain/transaction_evaluation_state.hpp>

namespace bts { namespace blockchain {

   struct block_record : public bts::blockchain::digest_block
   {
      block_record():block_size(0){}
      block_record( const digest_block& b, uint64_t s, const fc::ripemd160& r )
      :digest_block(b),block_size(s),random_seed(r){}

      uint64_t       block_size;
      fc::ripemd160  random_seed;
   };


   struct transaction_record : public transaction_evaluation_state
   {
      transaction_record(){}

      transaction_record( const transaction_location& loc, 
                          const transaction_evaluation_state& s )
      :transaction_evaluation_state(s),chain_location(loc){}

      transaction_location chain_location;
   };

   typedef optional<transaction_record> otransaction_record;
   typedef optional<block_record>       oblock_record;


} }

FC_REFLECT_DERIVED( bts::blockchain::block_record, 
                    (bts::blockchain::digest_block), 
                    (block_size)
                    (random_seed) )

FC_REFLECT_DERIVED( bts::blockchain::transaction_record, 
                    (bts::blockchain::transaction_evaluation_state), 
                    (chain_location) );
