#include <bts/blockchain/address.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/small_hash.hpp>
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
       ilog( "${signed_addr}", ("signed_addr",r) );
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

} }
namespace fc {
   void to_variant( const bts::blockchain::trx_output& var,  variant& vo )
   {
      try {
        fc::mutable_variant_object obj;
        obj["amount"]     = var.amount; 
        obj["claim_func"] = var.claim_func;
        // TODO: convert this over to a factory
        switch( int(var.claim_func) )
        {
           case bts::blockchain::claim_by_pts:
              obj["claim_data"] = fc::raw::unpack<bts::blockchain::claim_by_pts_output>(var.claim_data);
              break;
           case bts::blockchain::claim_by_signature:
              obj["claim_data"] = fc::raw::unpack<bts::blockchain::claim_by_signature_output>(var.claim_data);
              break;
           case bts::blockchain::claim_by_multi_sig:
              obj["claim_data"] = fc::raw::unpack<bts::blockchain::claim_by_multi_sig_output>(var.claim_data);
              break;
           case bts::blockchain::claim_by_password:
              obj["claim_data"] = fc::raw::unpack<bts::blockchain::claim_by_password_output>(var.claim_data);
              break;
           case bts::blockchain::claim_name:
              obj["claim_data"] = fc::raw::unpack<bts::blockchain::claim_name_output>(var.claim_data);
              break;
           case bts::blockchain::claim_fire_delegate:
              obj["claim_data"] = fc::raw::unpack<bts::blockchain::claim_fire_delegate_output>(var.claim_data);
              break;
           case bts::blockchain::null_claim_type:
              break;
        };
        vo = std::move(obj);
      } FC_RETHROW_EXCEPTIONS( warn, "unable to convert output to variant" ) 
   }

   /** @todo update this to use a factory and be polymorphic for derived blockchains */
   void from_variant( const variant& var,  bts::blockchain::trx_output& vo )
   {
       fc::mutable_variant_object obj(var);

       from_variant(obj["amount"] ,vo.amount);
       from_variant(obj["claim_func"], vo.claim_func);

       // TODO: convert this over to a factory
       switch( (bts::blockchain::claim_type_enum)int(vo.claim_func) )
       {
	        case bts::blockchain::claim_by_pts:
		      {
			      bts::blockchain::claim_by_pts_output c;
			      from_variant(obj["claim_data"], c);
			      vo.claim_data = fc::raw::pack(c);
			      break;
		      }
          case bts::blockchain::claim_by_signature:
          {
                  bts::blockchain::claim_by_signature_output c;
                  from_variant(obj["claim_data"], c);
                  vo.claim_data = fc::raw::pack(c);
                  break;
          }
          case bts::blockchain::claim_by_multi_sig:
          {
                  bts::blockchain::claim_by_multi_sig_output c;
                  from_variant(obj["claim_data"], c);
                  vo.claim_data = fc::raw::pack(c);
                  break;
          }
          case bts::blockchain::claim_by_password:
          {
                  bts::blockchain::claim_by_password_output c;
                  from_variant(obj["claim_data"], c);
                  vo.claim_data = fc::raw::pack(c);
                  break;
          }
          case bts::blockchain::claim_name:
          {
                  bts::blockchain::claim_name_output c;
                  from_variant(obj["claim_data"], c);
                  vo.claim_data = fc::raw::pack(c);
                  break;
          }
          case bts::blockchain::claim_fire_delegate:
          {
                  bts::blockchain::claim_fire_delegate_output c;
                  from_variant(obj["claim_data"], c);
                  vo.claim_data = fc::raw::pack(c);
                  break;
          }
          case bts::blockchain::null_claim_type: break;
       };
   }
};
