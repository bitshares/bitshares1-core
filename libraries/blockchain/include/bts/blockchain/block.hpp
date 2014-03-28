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
       :version(0),block_num(-1),next_fee(1),total_shares(0){}
      
       /** @return digest used for signing */
       fc::sha256          digest()const;
       static uint64_t     calculate_next_fee( uint64_t prev_fee, uint64_t block_size );
       static uint64_t     min_fee();
      
       uint8_t             version;
       uint32_t            block_num;
       block_id_type       prev;
       fc::time_point_sec  timestamp;       ///< seconds from 1970
       uint64_t            next_fee;        ///< adjusted based upon average block size.
       uint64_t            total_shares; 
       uint160             trx_mroot;       ///< merkle root of trx included in block, required for light client validation
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

      digest_block( const signed_block_header& b, const signed_transactions& trxs, const signed_transactions& determinsitic_trxs );

      digest_block(){}

      uint160 calculate_merkle_root()const;


      std::vector<uint160>  trx_ids; 
      std::vector<uint160>  determinsitic_ids; 
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
      uint160 calculate_merkle_root( const signed_transactions& determinsitic_trxs )const;

      signed_transactions trxs;
   };

} } // bts::blockchain

namespace fc 
{
   void to_variant( const bts::blockchain::trx_output& var,  variant& vo );
   void from_variant( const variant& var,  bts::blockchain::trx_output& vo );
}

FC_REFLECT( bts::blockchain::block_header,  (version)(block_num)(prev)(timestamp)(next_fee)(total_shares)(trx_mroot) )
FC_REFLECT_DERIVED( bts::blockchain::signed_block_header, (bts::blockchain::block_header), (trustee_signature) )
FC_REFLECT_DERIVED( bts::blockchain::digest_block,  (bts::blockchain::signed_block_header), (trx_ids)(determinsitic_ids) )
FC_REFLECT_DERIVED( bts::blockchain::trx_block,   (bts::blockchain::signed_block_header),  (trxs) )

