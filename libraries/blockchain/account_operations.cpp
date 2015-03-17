#include <bts/blockchain/account_operations.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/pending_chain_state.hpp>
#include <bts/blockchain/transaction_evaluation_state.hpp>
#include <fc/time.hpp>

#include <bts/blockchain/fork_blocks.hpp>

namespace bts { namespace blockchain {

   bool register_account_operation::is_delegate()const
   {
       return delegate_pay_rate <= 100;
   }

   void register_account_operation::evaluate( transaction_evaluation_state& eval_state )const
   { try {
      if( !eval_state.pending_state()->is_valid_account_name( this->name ) )
         FC_CAPTURE_AND_THROW( invalid_account_name, (name) );

      oaccount_record current_account = eval_state.pending_state()->get_account_record( this->name );
      if( current_account.valid() )
          FC_CAPTURE_AND_THROW( account_already_registered, (name) );

      current_account = eval_state.pending_state()->get_account_record( this->owner_key );
      if( current_account.valid() )
          FC_CAPTURE_AND_THROW( account_key_in_use, (this->owner_key)(current_account) );

      current_account = eval_state.pending_state()->get_account_record( this->active_key );
      if( current_account.valid() )
          FC_CAPTURE_AND_THROW( account_key_in_use, (this->active_key)(current_account) );

      if( eval_state.pending_state()->get_parent_account_name( this->name ).valid() )
      {
          if( !eval_state.any_parent_has_signed( this->name ) )
              FC_CAPTURE_AND_THROW( missing_parent_account_signature, (*this) );
      }

      const time_point_sec now = eval_state.pending_state()->now();

      account_record new_record;
      new_record.id                = eval_state.pending_state()->new_account_id();
      new_record.name              = this->name;
      new_record.public_data       = this->public_data;
      new_record.owner_key         = this->owner_key;
      new_record.set_active_key( now, this->active_key );
      new_record.registration_date = now;
      new_record.last_update       = now;
      new_record.meta_data         = this->meta_data;

      if( !new_record.meta_data.valid() )
          new_record.meta_data = account_meta_info( titan_account );

      if( this->is_delegate() )
      {
          new_record.delegate_info = delegate_stats();
          new_record.delegate_info->pay_rate = this->delegate_pay_rate;
          new_record.set_signing_key( eval_state.pending_state()->get_head_block_num(), this->active_key );

          const asset reg_fee( eval_state.pending_state()->get_delegate_registration_fee( this->delegate_pay_rate ), 0 );
          eval_state.min_fees[ reg_fee.asset_id ] += reg_fee.amount;
      }

      eval_state.pending_state()->store_account_record( new_record );
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

   bool update_account_operation::is_retracted()const
   {
       return this->active_key.valid() && *this->active_key == public_key_type();
   }

   bool update_account_operation::is_delegate()const
   {
       return delegate_pay_rate <= 100;
   }

   void update_account_operation::evaluate( transaction_evaluation_state& eval_state )const
   { try {
      if( eval_state.pending_state()->get_head_block_num() < BTS_V0_6_0_FORK_BLOCK_NUM )
          return evaluate_v1( eval_state );

      oaccount_record current_record = eval_state.pending_state()->get_account_record( this->account_id );
      if( !current_record.valid() )
          FC_CAPTURE_AND_THROW( unknown_account_id, (account_id) );

      if( current_record->is_retracted() )
          FC_CAPTURE_AND_THROW( account_retracted, (current_record) );

      // If updating active key
      if( this->active_key.valid() && *this->active_key != current_record->active_key() )
      {
          if( !this->is_retracted() )
          {
              const oaccount_record account_with_same_key = eval_state.pending_state()->get_account_record( *this->active_key );
              if( account_with_same_key.valid() )
                  FC_CAPTURE_AND_THROW( account_key_in_use, (*this->active_key)(account_with_same_key) );
          }
          else
          {
              if( current_record->is_delegate() && current_record->delegate_pay_balance() > 0 )
                  FC_CAPTURE_AND_THROW( pay_balance_remaining, (*current_record) );
          }

          if( !eval_state.check_signature( current_record->owner_key ) && !eval_state.any_parent_has_signed( current_record->name ) )
              FC_CAPTURE_AND_THROW( missing_signature, (*this) );

          current_record->set_active_key( eval_state.pending_state()->now(), *this->active_key );
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

   void withdraw_pay_operation::evaluate( transaction_evaluation_state& eval_state )const
   { try {
      if( this->amount <= 0 )
          FC_CAPTURE_AND_THROW( negative_withdraw, (amount) );

      const account_id_type account_id = abs( this->account_id );
      oaccount_record account = eval_state.pending_state()->get_account_record( account_id );
      if( !account.valid() )
          FC_CAPTURE_AND_THROW( unknown_account_id, (account_id) );

      if( !account->is_delegate() )
          FC_CAPTURE_AND_THROW( not_a_delegate, (account) );

      if( account->delegate_info->pay_balance < this->amount )
         FC_CAPTURE_AND_THROW( insufficient_funds, (account)(amount) );

      if( !eval_state.account_or_any_parent_has_signed( *account ) )
          FC_CAPTURE_AND_THROW( missing_signature, (*this) );

      account->delegate_info->pay_balance -= this->amount;
      eval_state.delegate_vote_deltas[ account_id ] -= this->amount;
      eval_state.add_balance( asset( this->amount, 0 ) );

      eval_state.pending_state()->store_account_record( *account );
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

   void update_signing_key_operation::evaluate( transaction_evaluation_state& eval_state )const
   { try {
      oaccount_record account_rec = eval_state.pending_state()->get_account_record( this->account_id );
      if( !account_rec.valid() )
          FC_CAPTURE_AND_THROW( unknown_account_id, (account_id) );

      if( account_rec->is_retracted() )
          FC_CAPTURE_AND_THROW( account_retracted, (*account_rec) );

      if( !account_rec->is_delegate() )
          FC_CAPTURE_AND_THROW( not_a_delegate, (*account_rec) );

      oaccount_record existing_record = eval_state.pending_state()->get_account_record( this->signing_key );
      if( existing_record.valid() )
          FC_CAPTURE_AND_THROW( account_key_in_use, (*existing_record) );

      if( !eval_state.check_signature( account_rec->signing_address() ) && !eval_state.account_or_any_parent_has_signed( *account_rec ) )
          FC_CAPTURE_AND_THROW( missing_signature, (*this) );

      account_rec->set_signing_key( eval_state.pending_state()->get_head_block_num(), this->signing_key );
      account_rec->last_update = eval_state.pending_state()->now();

      eval_state.pending_state()->store_account_record( *account_rec );
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

} } // bts::blockchain
