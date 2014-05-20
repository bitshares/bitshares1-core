#include <bts/blockchain/fire_operation.hpp>
#include <fc/io/raw.hpp>

namespace bts { namespace blockchain {
   const operation_type_enum fire_delegate_operation::type      = fire_delegate_op_type;

   digest_type      delegate_testimony::digest()const
   {
      fc::sha256::encoder enc;
      fc::raw::pack( enc, *this );
      return enc.result();
   }

   void             signed_delegate_testimony::sign( const fc::ecc::private_key& signer )
   {
      delegate_signature = signer.sign_compact( digest() );
   }

   public_key_type  signed_delegate_testimony::signee()const
   {
      return fc::ecc::public_key( delegate_signature, digest()  );
   }

   fire_delegate_operation::fire_delegate_operation( name_id_type delegate_id_arg, const multiple_block_proof& proof )
   :delegate_id( delegate_id_arg )
   {
      reason = multiple_blocks_signed;
      data = fc::raw::pack( proof );
   }

   fire_delegate_operation:: fire_delegate_operation( const signed_delegate_testimony& testimony )
   :delegate_id( testimony.delegate_id )
   {
      reason = invalid_testimony;
      data   = fc::raw::pack( testimony );
   }

} } // bts::blockchain
