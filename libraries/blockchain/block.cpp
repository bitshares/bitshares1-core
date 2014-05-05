#include <algorithm>

#include <fc/io/raw.hpp>
#include <fc/reflect/variant.hpp>

#include <bts/blockchain/block.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/small_hash.hpp>

#include <bts/blockchain/difficulty.hpp>
namespace bts { namespace blockchain  {


   digest_block::digest_block( const signed_block_header& b,
                               const signed_transactions& trxs,
                               const signed_transactions& determinstic_trxs )
   :signed_block_header(b)
   {
      trx_ids.reserve( trxs.size() );
      for( auto trx : trxs )
         trx_ids.push_back( trx.id() );
      for( auto trx : determinstic_trxs)
         deterministic_ids.push_back( trx.id() );
   }


  block_id_type trx_block::calculate_merkle_root( const signed_transactions& determinstic_trxs )const
  {
     if( trxs.size() == 0 ) return block_id_type();
     if( trxs.size() == 1 ) return trxs.front().id();

     std::vector<block_id_type> layer_one;
     for( const signed_transaction& trx : trxs )
        layer_one.push_back( trx.id() );
     for( const signed_transaction& trx : determinstic_trxs )
        layer_one.push_back( trx.id() );

     std::vector<block_id_type> layer_two;
     while( layer_one.size() > 1 )
     {
        if( layer_one.size() % 2 == 1 )
        {
          layer_one.push_back( block_id_type() );
        }

        static_assert( sizeof(block_id_type[2]) == 40, "validate there is no padding between array items" );
        for( uint32_t i = 0; i < layer_one.size(); i += 2 )
        {
            layer_two.push_back(  small_hash( (char*)&layer_one[i], 2*sizeof(block_id_type) ) );
        }

        layer_one = std::move(layer_two);
     }
     return layer_one.front();
  }

  /**
   *  The min fee is specified in shares, but for the purposes of the block header there is
   *  higher percision.
   */
  uint64_t block_header::min_fee()
  {
     return BTS_BLOCKCHAIN_MIN_FEE * 1000;
  }

  uint64_t block_header::calculate_next_fee( uint64_t prev_fee, uint64_t block_size )
  {
     uint64_t next_fee_base = block_size * prev_fee / BTS_BLOCKCHAIN_TARGET_BLOCK_SIZE;
     uint64_t next_fee = ((BTS_BLOCKCHAIN_BLOCKS_PER_DAY-1)*prev_fee + next_fee_base) / BTS_BLOCKCHAIN_BLOCKS_PER_DAY;
     return std::max<uint64_t>(next_fee,min_fee());
  }
  uint64_t block_header::calculate_next_reward( uint64_t prev_reward, uint64_t block_fees )
  {
     uint64_t next_reward_base = (block_fees/10) * prev_reward / BTS_BLOCKCHAIN_TARGET_BLOCK_SIZE;
     uint64_t next_reward = ((BTS_BLOCKCHAIN_BLOCKS_PER_DAY-1)*prev_reward + next_reward_base) / BTS_BLOCKCHAIN_BLOCKS_PER_DAY;
     return std::max<uint64_t>(next_reward,BTS_BLOCKCHAIN_MIN_REWARD);
  }


  size_t trx_block::block_size()const
  {
     fc::datastream<size_t> ds;
     fc::raw::pack( ds, *this );
     return ds.tellp();
  }

  /**
   *  @return the digest of the block header used to evaluate the proof of work
   */
  block_id_type signed_block_header::id()const
  {
     fc::sha512::encoder enc;
     fc::raw::pack( enc, *this );
     return small_hash( enc.result() );
  }

  /**
   *  @return the digest of the block header used to evaluate the proof of work
   */
  fc::sha256 block_header::digest()const
  {
     fc::sha256::encoder enc;
     fc::raw::pack( enc, *this );
     return enc.result();
  }

  void signed_block_header::sign( const fc::ecc::private_key& k )
  {
      trustee_signature = k.sign_compact( digest() );
  }
  fc::ecc::public_key  signed_block_header::signee()const
  {
     return fc::ecc::public_key( trustee_signature, digest() );
  }

} } // bts::blockchain
