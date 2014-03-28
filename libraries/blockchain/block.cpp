#include <bts/blockchain/block.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/small_hash.hpp>

#include <bts/blockchain/difficulty.hpp>
#include <fc/io/raw.hpp>
#include <fc/reflect/variant.hpp>
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
         determinsitic_ids.push_back( trx.id() );
   }


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
