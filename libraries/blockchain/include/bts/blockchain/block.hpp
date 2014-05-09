#pragma once

#include <bts/blockchain/transaction.hpp>

namespace bts { namespace blockchain {

   struct block_header
   {
       digest_type          digest()const;

       block_header():block_num(0),fee_rate(0),delegate_pay_rate(0){}

       int64_t next_fee( int64_t prev_fee_rate, size_t block_size )const;
       share_type next_delegate_pay( share_type prev_fee_rate, share_type block_fees )const;

       block_id_type        previous;
       uint32_t             block_num;
       int64_t              fee_rate;
       share_type           delegate_pay_rate;
       fc::time_point_sec   timestamp;
       digest_type          transaction_digest;
   };

   struct signed_block_header : public block_header
   {
       block_id_type        id()const;
       bool validate_signee( const public_key_type& expected_signee )const;
       void sign( const fc::ecc::private_key& signeer );

       signature_type signee;
   };

   struct digest_block : public signed_block_header
   {
       digest_block( const signed_block_header& c )
       :signed_block_header(c){}

       digest_block(){}

       bool                             validate_digest()const;
       bool                             validate_unique()const;
       digest_type                      calculate_transaction_digest()const;
       std::vector<transaction_id_type> user_transaction_ids;
   };

   struct full_block : public signed_block_header
   {
       size_t               block_size()const;
       signed_transactions  user_transactions;

       operator digest_block()const;
   };

} } // bts::blockchain

FC_REFLECT( bts::blockchain::block_header, 
            (previous)(block_num)(fee_rate)(delegate_pay_rate)(timestamp)(transaction_digest) )
FC_REFLECT_DERIVED( bts::blockchain::signed_block_header, (bts::blockchain::block_header), (signee) )
FC_REFLECT_DERIVED( bts::blockchain::digest_block, (bts::blockchain::signed_block_header), (user_transaction_ids) )
FC_REFLECT_DERIVED( bts::blockchain::full_block, (bts::blockchain::signed_block_header), (user_transactions) )
