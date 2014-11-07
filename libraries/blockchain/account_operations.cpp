#include <bts/blockchain/account_operations.hpp>
#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/transaction_evaluation_state.hpp>
#include <fc/time.hpp>

namespace bts { namespace blockchain {

   string get_parent_account_name( const string& child )
   {
      const auto pos = child.find( '.' );
      if( pos == string::npos )
          return string();
      return child.substr( pos + 1 );
   }

   bool register_account_operation::is_delegate()const
   {
       return delegate_pay_rate <= 100;
   }

   void register_account_operation::evaluate( transaction_evaluation_state& eval_state )
   { try {
      if( !eval_state._current_state->is_valid_account_name( this->name ) )
         FC_CAPTURE_AND_THROW( invalid_account_name, (name) );

      oaccount_record current_account = eval_state._current_state->get_account_record( this->name );
      if( current_account.valid() )
          FC_CAPTURE_AND_THROW( account_already_registered, (name) );

      current_account = eval_state._current_state->get_account_record( this->owner_key );
      if( current_account.valid() )
          FC_CAPTURE_AND_THROW( account_key_in_use, (this->owner_key)(current_account) );

      current_account = eval_state._current_state->get_account_record( this->active_key );
      if( current_account.valid() )
          FC_CAPTURE_AND_THROW( account_key_in_use, (this->active_key)(current_account) );

      string parent_name = get_parent_account_name( this->name );
      if( !parent_name.empty() )
      {
         const oaccount_record parent_record = eval_state._current_state->get_account_record( parent_name );
         if( !parent_record.valid() )
             FC_CAPTURE_AND_THROW( unknown_account_name, (parent_name) );

         if( parent_record->is_retracted() )
            FC_CAPTURE_AND_THROW( parent_account_retracted, (parent_record) );

         // TODO: Allow any parent signature like in update_account_operation below
         if( !eval_state.check_signature( parent_record->active_key() ) )
            FC_CAPTURE_AND_THROW( missing_parent_account_signature, (parent_record) );
      }

      const time_point_sec now = eval_state._current_state->now();

      account_record new_record;
      new_record.id                = eval_state._current_state->new_account_id();
      new_record.name              = this->name;
      new_record.public_data       = this->public_data;
      new_record.owner_key         = this->owner_key;
      new_record.set_active_key( now, this->active_key );
      new_record.registration_date = now;
      new_record.last_update       = now;
      new_record.meta_data         = this->meta_data;

      if( this->is_delegate() )
      {
          new_record.delegate_info = delegate_stats();
          new_record.delegate_info->pay_rate = this->delegate_pay_rate;
          new_record.delegate_info->block_signing_key = this->active_key;

          const asset reg_fee( eval_state._current_state->get_delegate_registration_fee( this->delegate_pay_rate ), 0 );
          eval_state.required_fees += reg_fee;
      }

      eval_state._current_state->store_account_record( new_record );
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

   bool update_account_operation::is_delegate()const
   {
       return delegate_pay_rate <= 100;
   }

   void update_account_operation::evaluate( transaction_evaluation_state& eval_state )
   { try {
      oaccount_record current_record = eval_state._current_state->get_account_record( this->account_id );
      if( !current_record.valid() )
          FC_CAPTURE_AND_THROW( unknown_account_id, (account_id) );

      if( current_record->is_retracted() )
          FC_CAPTURE_AND_THROW( account_retracted, (current_record) );

      const auto can_update_active_key = [&]() -> bool
      {
          if( eval_state.check_signature( current_record->owner_key ) )
              return true;

          string parent_name = get_parent_account_name( current_record->name );
          while( !parent_name.empty() )
          {
              const oaccount_record parent_record = eval_state._current_state->get_account_record( parent_name );
              if( !parent_record.valid() )
                  return false;

              if( parent_record->is_retracted() )
                  return false;

              if( eval_state.check_signature( parent_record->active_key() ) )
                  return true;

              if( eval_state.check_signature( parent_record->owner_key ) )
                  return true;

              parent_name = get_parent_account_name( parent_name );
          }

          return false;
      };

      if( this->active_key.valid() && *this->active_key != current_record->active_key() )
      {
          const oaccount_record account_with_same_key = eval_state._current_state->get_account_record( *this->active_key );
          if( account_with_same_key.valid() )
              FC_CAPTURE_AND_THROW( account_key_in_use, (*this->active_key)(account_with_same_key) );

          if( !can_update_active_key() )
              FC_CAPTURE_AND_THROW( missing_signature, (*this) );

          current_record->set_active_key( eval_state._current_state->now(), *this->active_key );

#ifndef WIN32
#warning [FUTURE HARDFORK] Until block signing keys can be changed, they must always be equal to the active key
#endif
          if( current_record->is_delegate() )
              current_record->delegate_info->block_signing_key = current_record->active_key();
      }
      else
      {
          if( !eval_state.check_signature( current_record->active_key() ) && !can_update_active_key() )
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
              current_record->delegate_info->block_signing_key = current_record->active_key();
              const asset reg_fee( eval_state._current_state->get_delegate_registration_fee( this->delegate_pay_rate ), 0 );
              eval_state.required_fees += reg_fee;
          }
      }

      current_record->last_update = eval_state._current_state->now();

      eval_state._current_state->store_account_record( *current_record );
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

   void withdraw_pay_operation::evaluate( transaction_evaluation_state& eval_state )
   { try {
      if( this->amount <= 0 )
          FC_CAPTURE_AND_THROW( negative_withdraw, (amount) );

      const account_id_type account_id = abs( this->account_id );
      oaccount_record account = eval_state._current_state->get_account_record( account_id );
      if( !account.valid() )
          FC_CAPTURE_AND_THROW( unknown_account_id, (account_id) );

      if( !account->is_delegate() )
          FC_CAPTURE_AND_THROW( not_a_delegate, (account) );

      if( account->delegate_info->pay_balance < this->amount )
         FC_CAPTURE_AND_THROW( insufficient_funds, (account)(amount) );

      // TODO: Allow any parent signature like in update_account_operation above
      if( !eval_state.check_signature( account->active_key() ) && !eval_state.check_signature( account->owner_key ) )
          FC_CAPTURE_AND_THROW( missing_signature, (*account) );

      account->delegate_info->pay_balance -= this->amount;
      eval_state.net_delegate_votes[ account_id ].votes_for -= this->amount;
      eval_state.add_balance( asset( this->amount, 0 ) );

      eval_state._current_state->store_account_record( *account );
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

   void link_account_operation::evaluate( transaction_evaluation_state& eval_state )
   { try {
      FC_ASSERT( !"Link account operation is not enabled yet!" );

      auto source_account_rec = eval_state._current_state->get_account_record( source_account );
      FC_ASSERT( source_account_rec.valid() );

      auto destination_account_rec = eval_state._current_state->get_account_record( destination_account );
      FC_ASSERT( destination_account_rec.valid() );

      if( !eval_state.check_signature( source_account_rec->active_key() ) )
      {
         FC_CAPTURE_AND_THROW( missing_signature, (source_account_rec->active_key()) );
      }

      // STORE LINK...
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

   void update_block_signing_key::evaluate( transaction_evaluation_state& eval_state )
   { try {
      FC_ASSERT( !"Update block signing key operation is not enabled yet!" );

      oaccount_record account_rec = eval_state._current_state->get_account_record( this->account_id );
      if( !account_rec.valid() )
          FC_CAPTURE_AND_THROW( unknown_account_id, (account_id) );

      if( account_rec->is_retracted() )
          FC_CAPTURE_AND_THROW( account_retracted, (*account_rec) );

      if( !account_rec->is_delegate() )
          FC_CAPTURE_AND_THROW( not_a_delegate, (*account_rec) );

      // TODO: Allow any parent signature like in update_account_operation above
      if( !eval_state.check_signature( account_rec->active_key() ) && !eval_state.check_signature( account_rec->owner_key ) )
          FC_CAPTURE_AND_THROW( missing_signature, (*account_rec) );

      account_rec->delegate_info->block_signing_key = this->block_signing_key;
      account_rec->last_update = eval_state._current_state->now();

      eval_state._current_state->store_account_record( *account_rec );
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

} } // bts::blockchain
