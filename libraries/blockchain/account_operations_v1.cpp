#include <bts/blockchain/account_operations.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/pending_chain_state.hpp>
#include <bts/blockchain/transaction_evaluation_state.hpp>
#include <fc/time.hpp>

namespace bts { namespace blockchain {

void update_account_operation::evaluate_v1( transaction_evaluation_state& eval_state )const
{ try {
   oaccount_record current_record = eval_state.pending_state()->get_account_record( this->account_id );
   if( !current_record.valid() )
       FC_CAPTURE_AND_THROW( unknown_account_id, (account_id) );

   if( current_record->is_retracted() )
       FC_CAPTURE_AND_THROW( account_retracted, (current_record) );

   // If updating active key
   if( this->active_key.valid() && *this->active_key != current_record->active_key() )
   {
       const oaccount_record account_with_same_key = eval_state.pending_state()->get_account_record( *this->active_key );
       if( account_with_same_key.valid() )
           FC_CAPTURE_AND_THROW( account_key_in_use, (*this->active_key)(account_with_same_key) );

       if( !eval_state.check_signature( current_record->owner_key ) && !eval_state.any_parent_has_signed( current_record->name ) )
           FC_CAPTURE_AND_THROW( missing_signature, (*this) );

       current_record->set_active_key( eval_state.pending_state()->now(), *this->active_key );

       FC_ASSERT( !current_record->is_retracted() || !current_record->is_delegate() );

       if( current_record->is_delegate() )
           current_record->set_signing_key( eval_state.pending_state()->get_head_block_num(), current_record->active_key() );
   }
   else
   {
       if( !eval_state.account_or_any_parent_has_signed( *current_record ) )
           FC_CAPTURE_AND_THROW( missing_signature, (*this) );
   }

   if( this->public_data.valid() )
       current_record->public_data = *this->public_data;

   // Delegates accounts cannot revert to a normal account
   if( current_record->is_delegate() )
       FC_ASSERT( this->is_delegate() );

   if( this->is_delegate() )
   {
       if( current_record->is_delegate() )
       {
           // Delegates cannot increase their pay rate
           FC_ASSERT( this->delegate_pay_rate <= current_record->delegate_info->pay_rate );
           current_record->delegate_info->pay_rate = this->delegate_pay_rate;
       }
       else
       {
           current_record->delegate_info = delegate_stats();
           current_record->delegate_info->pay_rate = this->delegate_pay_rate;
           current_record->set_signing_key( eval_state.pending_state()->get_head_block_num(), current_record->active_key() );

           const asset reg_fee( eval_state.pending_state()->get_delegate_registration_fee( this->delegate_pay_rate ), 0 );
           eval_state.min_fees[ reg_fee.asset_id ] += reg_fee.amount;
       }
   }

   current_record->last_update = eval_state.pending_state()->now();

   eval_state.pending_state()->store_account_record( *current_record );
} FC_CAPTURE_AND_RETHROW( (*this) ) }

} } // bts::blockchain
