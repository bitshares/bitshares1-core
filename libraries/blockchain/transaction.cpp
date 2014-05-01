#include <bts/blockchain/address.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/small_hash.hpp>
#include <bts/blockchain/output_factory.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/io/raw.hpp>

#include <fc/log/logger.hpp>

namespace bts { namespace blockchain {

   fc::sha256 transaction::digest()const
   {
      fc::sha256::encoder enc;
      fc::raw::pack( enc, *this );
      return enc.result();
   }

   std::unordered_set<address> signed_transaction::get_signed_addresses()const
   {
       auto dig = digest(); 
       std::unordered_set<address> r;
       for( auto itr = sigs.begin(); itr != sigs.end(); ++itr )
       {
            r.insert( address(fc::ecc::public_key( *itr, dig )) );
       }
       return r;
   }

   std::unordered_set<pts_address> signed_transaction::get_signed_pts_addresses()const
   {
       auto dig = digest(); 
       std::unordered_set<pts_address> r;
       // add both compressed and uncompressed forms...
       for( auto itr = sigs.begin(); itr != sigs.end(); ++itr )
       {
            auto signed_key_data = fc::ecc::public_key( *itr, dig ).serialize();
            
            // note: 56 is the version bit of protoshares
            r.insert( pts_address(fc::ecc::public_key( signed_key_data),false,56) );
            r.insert( pts_address(fc::ecc::public_key( signed_key_data ),true,56) );
            // note: 5 comes from en.bitcoin.it/wiki/Vanitygen where version bit is 0
            r.insert( pts_address(fc::ecc::public_key( signed_key_data ),false,0) );
            r.insert( pts_address(fc::ecc::public_key( signed_key_data ),true,0) );
       }
       return r;
   }

   transaction_id_type signed_transaction::id()const
   {
      fc::sha512::encoder enc;
      fc::raw::pack( enc, *this );
      return small_hash( enc.result() );
   }

   void    signed_transaction::sign( const fc::ecc::private_key& k )
   {
    try {
      sigs.insert( k.sign_compact( digest() ) );  
     } FC_RETHROW_EXCEPTIONS( warn, "error signing transaction", ("trx", *this ) );
   }

   size_t signed_transaction::size()const
   {
      fc::datastream<size_t> ds;
      fc::raw::pack(ds,*this);
      return ds.tellp();
   }

   output_factory& output_factory::instance()
   {
      static std::unique_ptr<output_factory> inst( new output_factory() );
      return *inst;
   }

   void output_factory::to_variant( const trx_output& in, fc::variant& output )
   {
      auto converter_itr = _converters.find( in.claim_func );
      FC_ASSERT( converter_itr != _converters.end() );
      converter_itr->second->to_variant( in, output );
   }
   void output_factory::from_variant( const fc::variant& in, trx_output& output )
   {
      auto obj = in.get_object();
      output.claim_func = obj["claim_func"].as_int64();

      auto converter_itr = _converters.find( output.claim_func );
      FC_ASSERT( converter_itr != _converters.end() );
      converter_itr->second->from_variant( in, output );
   }
} } // bts::blockchain

namespace fc {
   void to_variant( const bts::blockchain::trx_output& var,  variant& vo )
   {
      bts::blockchain::output_factory::instance().to_variant( var, vo );
   }

   /** @todo update this to use a factory and be polymorphic for derived blockchains */
   void from_variant( const variant& var,  bts::blockchain::trx_output& vo )
   {
      bts::blockchain::output_factory::instance().from_variant( var, vo );
   }
};
