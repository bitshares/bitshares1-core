#pragma once

#include <bts/blockchain/transaction.hpp>

namespace bts { namespace blockchain {

   struct block_header
   {
       digest_type  digest()const;

       block_id_type        previous;
       uint32_t             block_num = 0;
       fc::time_point_sec   timestamp;
       digest_type          transaction_digest;
       /** used for random number generation on the blockchain */
       secret_hash_type     next_secret_hash;
       secret_hash_type     previous_secret;
   };

   struct signed_block_header : public block_header
   {
       block_id_type    id()const;
       bool             validate_signee( const fc::ecc::public_key& expected_signee, bool enforce_canonical = false )const;
       public_key_type  signee( bool enforce_canonical = false )const;
       void             sign( const fc::ecc::private_key& signer );

       signature_type delegate_signature;
   };

   struct full_block : public signed_block_header
   {
       size_t               block_size()const;

       signed_transactions  user_transactions;
   };

   struct digest_block : public signed_block_header
   {
       digest_block(){}
       digest_block( const full_block& block_data );

       digest_type                      calculate_transaction_digest()const;
       bool                             validate_digest()const;
       bool                             validate_unique()const;

       std::vector<transaction_id_type> user_transaction_ids;
   };

} } // bts::blockchain

FC_REFLECT( bts::blockchain::block_header,
            (previous)(block_num)(timestamp)(transaction_digest)(next_secret_hash)(previous_secret) )
FC_REFLECT_DERIVED( bts::blockchain::signed_block_header, (bts::blockchain::block_header), (delegate_signature) )
FC_REFLECT_DERIVED( bts::blockchain::full_block, (bts::blockchain::signed_block_header), (user_transactions) )
FC_REFLECT_DERIVED( bts::blockchain::digest_block, (bts::blockchain::signed_block_header), (user_transaction_ids) )
