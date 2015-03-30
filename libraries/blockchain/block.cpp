#include <bts/blockchain/block.hpp>
#include <algorithm>

#include <bts/blockchain/fork_blocks.hpp>

namespace bts { namespace blockchain {

   digest_type block_header::digest()const
   {
      fc::sha256::encoder enc;
      fc::raw::pack( enc, *this );

      if( block_num >= BTS_V0_9_0_FORK_BLOCK_NUM )
          fc::raw::pack( enc, uint32_t( 0 ) );

      return enc.result();
   }

   block_id_type signed_block_header::id()const
   {
      fc::sha512::encoder enc;
      fc::raw::pack( enc, *this );

      if( block_num >= BTS_V0_9_0_FORK_BLOCK_NUM )
          fc::raw::pack( enc, uint32_t( 0 ) );

      return fc::ripemd160::hash( enc.result() );
   }

   bool signed_block_header::validate_signee( const fc::ecc::public_key& expected_signee, bool enforce_canonical )const
   {
      return fc::ecc::public_key( delegate_signature, digest(), enforce_canonical ) == expected_signee;
   }

   public_key_type signed_block_header::signee( bool enforce_canonical )const
   {
      return fc::ecc::public_key( delegate_signature, digest(), enforce_canonical );
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

   digest_block::digest_block( const full_block& block_data )
   {
      (signed_block_header&)*this = block_data;
      user_transaction_ids.reserve( block_data.user_transactions.size() );
      for( const auto& item : block_data.user_transactions )
         user_transaction_ids.push_back( item.id() );
   }

   digest_type digest_block::calculate_transaction_digest()const
   {
      fc::sha512::encoder enc;
      fc::raw::pack( enc, user_transaction_ids );
      return fc::sha256::hash( enc.result() );
   }

   bool digest_block::validate_digest()const
   {
      return calculate_transaction_digest() == transaction_digest;
   }

   bool digest_block::validate_unique()const
   {
      std::unordered_set<transaction_id_type> trx_ids;
      for( const auto& id : user_transaction_ids )
         if( !trx_ids.insert(id).second ) return false;
      return true;
   }

} } // bts::blockchain
