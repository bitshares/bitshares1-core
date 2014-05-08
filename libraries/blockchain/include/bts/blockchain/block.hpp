#pragma once
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/asset.hpp>
#include <fc/uint128.hpp>

namespace bts { namespace blockchain {

   typedef fc::uint160_t block_id_type;

   /**
    *  Light-weight summary of a block that links it to
    *  all prior blocks.  This summary does not contain
    *  the nonce because that information is provided by
    *  the block_proof struct which is a header plus
    *  proof of work.
    */
   struct block_header
   {
       block_header()
       :version(BTS_BLOCKCHAIN_VERSION),block_num(trx_num::invalid_block_num),next_fee(1),next_reward(0),total_shares(0){}

       /** @return digest used for signing */
       fc::sha256           digest()const;
       static uint64_t      calculate_next_fee( uint64_t prev_fee, uint64_t block_size );
       static uint64_t      calculate_next_reward( uint64_t prev_reward, uint64_t block_fee );
       static uint64_t      min_fee();

       /** block_header#next_fee is specified in units of .001 shares, so the
        * fee per byte is next_fee / 1000
        */
       int64_t              get_next_fee()const { return next_fee / 1000; }

       uint8_t              version;
       uint32_t             block_num;
       block_id_type        prev;
       fc::time_point_sec   timestamp; // Seconds from 1970

       /**
        * adjusted based upon average block size, this fee should be divided by 1000 to get shares per byte
        *
        * This field is represented as 1000 the actual fee so that rounding with the weighted
        * average can actually grow the fee.
        */
       uint64_t             next_fee;

       /**
        * adjusted based upon the average block reward, this reward should be calculated as:
        *
        * next_reward = (prev_block.next_reward * (BTS_BLOCKCHAIN_BLOCKS_PER_DAY-1) + total_fees/10) / BTS_BLOCKCHAIN_BLOCKS_PER_DAY
        *
        * This method was chosen to smooth out block rewards over 24 hours and ensure nice predictable results
        * for the delegates without any incentive for the delegates to delay transactions or ignore the
        * previous block.
        *
        * The unit of next_reward is shares.
        */
       uint64_t             next_reward;
       uint64_t             total_shares;
       block_id_type        trx_mroot; // Merkle root of trx included in block, required for light client validation
   };

   /**
    *  A blockheader must be signed by the trustee.
    */
   struct signed_block_header : public block_header
   {
      signed_block_header(){}
      signed_block_header( const block_header& h ):block_header(h){}
      void sign( const fc::ecc::private_key& k );

      /** hash of the block header, including the signature */
      block_id_type              id()const;
      /** @return the signer of this block header */
      fc::ecc::public_key        signee()const;
      fc::ecc::compact_signature trustee_signature;
   };

   /**
    * A block complete with the IDs of the transactions included
    * in the block.  This is useful for communicating summaries when
    * the other party already has all of the trxs.
    */
   struct digest_block : public signed_block_header
   {
      digest_block( const signed_block_header& b )
      :signed_block_header(b){}

      digest_block( const signed_block_header& b, const signed_transactions& trxs, const signed_transactions& deterministic_trxs );

      digest_block(){}

      block_id_type calculate_merkle_root()const;

      std::vector<transaction_id_type> trx_ids;
      /** Special transactions that any client can "know" from existing blockchain data. For example, market transactions, etc.
          These transactions are just computed locally by each client, they are not actually broadcast over the network. */
      std::vector<transaction_id_type> deterministic_ids;
   };

   /**
    *  A block that contains the full transactions rather than
    *  just the IDs of the transactions.
    */
   struct trx_block : public signed_block_header
   {
      trx_block( const signed_block_header& b )
      :signed_block_header(b){}

      trx_block( const block_header& b, std::vector<signed_transaction> trs )
      :signed_block_header(b),trxs( std::move(trs) ){}

      size_t block_size()const;

      trx_block(){}

     // operator digest_block()const;
      block_id_type calculate_merkle_root( const signed_transactions& deterministic_trxs )const;

      signed_transactions trxs;
   };

} } // bts::blockchain

namespace fc
{
   void to_variant( const bts::blockchain::trx_output& var,  variant& vo );
   void from_variant( const variant& var,  bts::blockchain::trx_output& vo );
}

FC_REFLECT( bts::blockchain::block_header,  (version)(block_num)(prev)(timestamp)(next_fee)(next_reward)(total_shares)(trx_mroot) )
FC_REFLECT_DERIVED( bts::blockchain::signed_block_header, (bts::blockchain::block_header), (trustee_signature) )
FC_REFLECT_DERIVED( bts::blockchain::digest_block,  (bts::blockchain::signed_block_header), (trx_ids)(deterministic_ids) )
FC_REFLECT_DERIVED( bts::blockchain::trx_block,   (bts::blockchain::signed_block_header),  (trxs) )
