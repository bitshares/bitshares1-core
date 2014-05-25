#include <bts/blockchain/block.hpp>
#include <bts/blockchain/config.hpp>
#include <fc/log/logger.hpp>
#include <algorithm>

namespace bts { namespace blockchain {

   digest_type block_header::digest()const
   {
      fc::sha256::encoder enc;
      fc::raw::pack( enc, *this );
      return enc.result();
   }

   block_id_type signed_block_header::id()const
   {
      fc::sha512::encoder enc;
      fc::raw::pack( enc, *this );
      return fc::ripemd160::hash( enc.result() );
   }

   bool signed_block_header::validate_signee( const fc::ecc::public_key& expected_signee )const
   { 
      return fc::ecc::public_key( delegate_signature, digest() ) == expected_signee;
   }
   public_key_type signed_block_header::signee()const
   { 
      return fc::ecc::public_key( delegate_signature, digest() );
   }

   void signed_block_header::sign( const fc::ecc::private_key& signer )
   { try {
      delegate_signature = signer.sign_compact( digest() );
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   size_t full_block::block_size()const
   {
      fc::datastream<size_t> ds;
      fc::raw::pack( ds, *this );
      return ds.tellp();
   }

   digest_type digest_block::calculate_transaction_digest()const
   {
      fc::sha512::encoder enc;
      fc::raw::pack( enc, user_transaction_ids );
      return fc::sha256::hash( enc.result() );
   }

   int64_t block_header::next_fee( int64_t current_fee, size_t block_size )const
   {
     uint64_t next_fee_base = block_size * current_fee / BTS_BLOCKCHAIN_TARGET_BLOCK_SIZE;
     uint64_t next_fee = ((BTS_BLOCKCHAIN_BLOCKS_PER_DAY-1)*current_fee + next_fee_base) / BTS_BLOCKCHAIN_BLOCKS_PER_DAY;
     return std::max<uint64_t>(next_fee,BTS_BLOCKCHAIN_MIN_FEE);
   }

   share_type block_header::next_delegate_pay( share_type current_pay, share_type block_fees )const
   {
     uint64_t next_pay = ((BTS_BLOCKCHAIN_BLOCKS_PER_DAY-1)*current_pay + (block_fees)) / BTS_BLOCKCHAIN_BLOCKS_PER_DAY;
     return next_pay;
   }

   full_block::operator digest_block()const
   {
      digest_block db( (signed_block_header&)*this );
      db.user_transaction_ids.reserve( user_transactions.size() );
      for( auto item : user_transactions )
         db.user_transaction_ids.push_back( item.id() );
      return db;
   }

   bool digest_block::validate_digest()const
   {
      return calculate_transaction_digest() == transaction_digest;
   }
   bool digest_block::validate_unique()const
   {
      std::unordered_set<transaction_id_type> trx_ids;
      for( auto id : user_transaction_ids )
         if( !trx_ids.insert(id).second ) return false;
      return true;
   }

} } // bts::blockchain
