#include <bts/blockchain/block.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/small_hash.hpp>

#include <bts/blockchain/difficulty.hpp>
#include <fc/io/raw.hpp>
#include <fc/reflect/variant.hpp>
namespace bts { namespace blockchain  {


   digest_block::digest_block( const block_header& b, const signed_transactions& trxs, const signed_transactions& determinstic_trxs )
   :block_header(b)
   {
      trx_ids.reserve( trxs.size() );
      for( auto trx : trxs ) 
         trx_ids.push_back( trx.id() );
      for( auto trx : determinstic_trxs) 
         determinsitic_ids.push_back( trx.id() );
   }

   /*
  trx_block::operator digest_block()const
  {
    digest_block b( (const block_header&)*this );
    b.trx_ids.reserve( trxs.size() );
    for( auto itr = trxs.begin(); itr != trxs.end(); ++itr )
    {
      b.trx_ids.push_back( itr->id() );
    }
    return b;
  }
  */


  uint160 trx_block::calculate_merkle_root( const signed_transactions& determinstic_trxs )const
  {
     if( trxs.size() == 0 ) return uint160();
     if( trxs.size() == 1 ) return trxs.front().id();

     std::vector<uint160> layer_one;
     for( const signed_transaction& trx : trxs )
        layer_one.push_back( trx.id() );
     for( const signed_transaction& trx : determinstic_trxs )
        layer_one.push_back( trx.id() );

     std::vector<uint160> layer_two;
     while( layer_one.size() > 1 )
     {
        if( layer_one.size() % 2 == 1 )
        {
          layer_one.push_back( uint160() );
        }

        static_assert( sizeof(uint160[2]) == 40, "validate there is no padding between array items" );
        for( uint32_t i = 0; i < layer_one.size(); i += 2 )
        {
            layer_two.push_back(  small_hash( (char*)&layer_one[i], 2*sizeof(uint160) ) );
        }

        layer_one = std::move(layer_two);
     }
     return layer_one.front();
  }

  /**
   * A block must have at least the required number of votes before the block chain
   * can be extended.  The number of votes required is a function of the available
   * votes.   The idea is to spread the available votes evently over the next 
   * 12 months.  
   */
  uint64_t block_header::get_required_votes( uint64_t prev_available_votes )const
  {
     uint64_t min_votes = prev_available_votes / BTS_BLOCKCHAIN_BLOCKS_PER_YEAR;
     if( min_votes > votes_cast ) 
        return min_votes - votes_cast;
     return 0;
  }
  int64_t block_header::min_votes()const { return std::max<int64_t>(1,available_votes / BTS_BLOCKCHAIN_BLOCKS_PER_YEAR); }

  /*
  uint64_t block_header::get_required_difficulty(uint64_t prev_difficulty,uint64_t prev_avail_cdays)const
  {
      prev_avail_cdays /= BLOCKS_PER_YEAR;
      uint64_t cdd = total_cdd > prev_avail_cdays ? prev_avail_cdays : total_cdd;
      uint64_t factor = prev_avail_cdays - cdd ;
      factor += 1;
      // as CDD approaches the average CDD per block the factor approaches 0
      // as CDD approaches 0 factor approaches total_shares^2
      // if we have the threshold CDD then there is no difficulty penalty...
      // otherwise the difficulty goes up dramatically.
      return prev_difficulty * factor;
  }
  */

  uint64_t block_header::get_difficulty()const
  {
     return bts::blockchain::difficulty(id());
  }

  uint64_t block_header::min_fee()
  {
     return BTS_BLOCKCHAIN_MIN_FEE;
  }

  uint64_t block_header::calculate_next_fee( uint64_t prev_fee, uint64_t block_size )
  {
     // 0.5% of 4 Million coins divided among 144 blocks per day for 365 blocks per year at 512KB per block
     // yields the min fee per byte.  
     uint64_t next_fee_base = block_size * prev_fee / (512*1024);
     uint64_t next_fee = (99*prev_fee + next_fee_base) / 100;
     return std::max<uint64_t>(next_fee,min_fee());
  }

  /*
  bool    block_header::validate_work()const
  {
     block_header tmp = *this;
     tmp.noncea = 0;
     tmp.nonceb = 0;
     auto tmp_id = tmp.id();
     auto seed = fc::sha256::hash( (char*)&tmp_id, sizeof(tmp_id) );
     return momentum_verify( seed, noncea, nonceb );
  }
  */

  size_t trx_block::block_size()const
  {
     fc::datastream<size_t> ds;
     fc::raw::pack( ds, *this );
     return ds.tellp();
  }

  /**
   *  @return the digest of the block header used to evaluate the proof of work
   */
  block_id_type block_header::id()const
  {
     fc::sha512::encoder enc;
     fc::raw::pack( enc, *this );
     return small_hash( enc.result() );
  }

} } // bts::blockchain
