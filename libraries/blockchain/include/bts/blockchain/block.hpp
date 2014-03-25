#pragma once
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/asset.hpp>
#include <fc/uint128.hpp>

namespace bts { namespace blockchain {

   typedef uint160  block_id_type;

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
       :version(0),block_num(-1),next_fee(1),next_difficulty(1),total_shares(0),available_votes(0),votes_cast(0),noncea(0),nonceb(0){}
      
       block_id_type       id()const;
       uint64_t            get_required_votes(uint64_t prev_avail_cdays)const;
       uint64_t            get_difficulty()const;
       bool                validate_work()const;
       static uint64_t     calculate_next_fee( uint64_t prev_fee, uint64_t block_size );
       static uint64_t     min_fee();
       int64_t             min_votes()const;
      
       uint8_t             version;
       block_id_type       prev;
       uint32_t            block_num;
       fc::time_point_sec  timestamp;       ///< seconds from 1970
       uint64_t            next_fee;        ///< adjusted based upon average block size.
       uint64_t            next_difficulty; ///< difficulty for the next block.
       uint64_t            total_shares; 
       uint64_t            available_votes; ///< total uncast votes available to the network
       uint64_t            votes_cast;      ///< votes cast this block
       uint160             trx_mroot;       ///< merkle root of trx included in block, required for light client validation
       uint32_t            noncea;          ///< used for proof of work
       uint32_t            nonceb;          ///< used for proof of work
   };

   /**
    * A block complete with the IDs of the transactions included
    * in the block.  This is useful for communicating summaries when
    * the other party already has all of the trxs.
    */
   struct digest_block : public block_header 
   {
      digest_block( const block_header& b )
      :block_header(b){}

      digest_block( const block_header& b, const signed_transactions& trxs, const signed_transactions& determinsitic_trxs );

      digest_block(){}

      uint160 calculate_merkle_root()const;


      std::vector<uint160>  trx_ids; 
      std::vector<uint160>  determinsitic_ids; 
   };

   /**
    *  A block that contains the full transactions rather than
    *  just the IDs of the transactions.
    */
   struct trx_block : public block_header
   {
      trx_block( const block_header& b )
      :block_header(b){}

      trx_block( const block_header& b, std::vector<signed_transaction> trs )
      :block_header(b),trxs( std::move(trs) ){}

      size_t block_size()const;

      trx_block(){}

     // operator digest_block()const;
      uint160 calculate_merkle_root( const signed_transactions& determinsitic_trxs )const;

      signed_transactions trxs;
   };

} } // bts::blockchain

namespace fc 
{
   void to_variant( const bts::blockchain::trx_output& var,  variant& vo );
   void from_variant( const variant& var,  bts::blockchain::trx_output& vo );
}

FC_REFLECT( bts::blockchain::block_header,  (version)(prev)(block_num)(timestamp)(next_difficulty)(next_fee)(total_shares)(available_votes)(votes_cast)(trx_mroot)(noncea)(nonceb) )
FC_REFLECT_DERIVED( bts::blockchain::digest_block,  (bts::blockchain::block_header),        (trx_ids)(determinsitic_ids) )
FC_REFLECT_DERIVED( bts::blockchain::trx_block,   (bts::blockchain::block_header),        (trxs) )

