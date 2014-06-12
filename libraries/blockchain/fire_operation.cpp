#include <bts/blockchain/fire_operation.hpp>
#include <bts/blockchain/transaction_evaluation_state.hpp>
#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/exceptions.hpp>
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


   void fire_delegate_operation::evaluate( transaction_evaluation_state& eval_state )
   { try {
       auto delegate_record = eval_state._current_state->get_account_record( this->delegate_id );
       if( !delegate_record ) FC_CAPTURE_AND_THROW( unknown_account_id, (delegate_id) );
       if( !delegate_record->is_delegate() ) FC_CAPTURE_AND_THROW( not_a_delegate, (delegate_record) );

       switch( (fire_delegate_operation::reason_type)this->reason )
       {
          case fire_delegate_operation::multiple_blocks_signed:
          {
             auto proof = fc::raw::unpack<multiple_block_proof>( this->data );

             FC_ASSERT( proof.first.id() != proof.second.id() );
             FC_ASSERT( proof.first.timestamp == proof.second.timestamp )
             FC_ASSERT( proof.first.signee() == proof.second.signee() )
             FC_ASSERT( proof.first.validate_signee( delegate_record->active_key() ) );

             // then fire the delegate
             // this maintains the invariant of total votes == total shares
             delegate_record->adjust_votes_against( delegate_record->votes_for() );
             delegate_record->adjust_votes_for( -delegate_record->votes_for() );
             eval_state._current_state->store_account_record( *delegate_record );
             break;
          }
          case fire_delegate_operation::invalid_testimony:
          {
             auto testimony = fc::raw::unpack<signed_delegate_testimony>( this->data );

             bool is_delegates_key = false;
             auto signee = testimony.signee();
             for( auto key : delegate_record->active_key_history )
             {
                if( signee == key.second )
                {
                   is_delegates_key = true;
                   break;
                }
             }
             if( !is_delegates_key ) FC_CAPTURE_AND_THROW( not_a_delegate_signature, (signee)(delegate_record) );

             auto trx_loc = eval_state._current_state->get_transaction( testimony.transaction_id );
             // delegate said it was valid, but it is invalid
             if( !trx_loc && testimony.valid )
             {
                delegate_record->adjust_votes_against( delegate_record->votes_for() );
                delegate_record->adjust_votes_for( -delegate_record->votes_for() );
                eval_state._current_state->store_account_record( *delegate_record );
             }
             else
             {
                FC_CAPTURE_AND_THROW( invalid_fire_operation );
             }
             break;
          }
       }
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

} } // bts::blockchain
