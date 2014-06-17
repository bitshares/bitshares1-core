#include <bts/blockchain/account_operations.hpp>
#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/transaction_evaluation_state.hpp>

namespace bts { namespace blockchain { 

   static const fc::microseconds one_year = fc::seconds( 60*60*24*365 );

   register_account_operation::register_account_operation( const std::string& n, 
                                                   const fc::variant& d, 
                                                   const public_key_type& owner, 
                                                   const public_key_type& active, bool as_delegate )
   :name(n),public_data(d),owner_key(owner),active_key(active),is_delegate(as_delegate){}


   string get_parent_account_name( const string& child )
   {
      auto pos = child.find( '.' );
      if( pos == string::npos ) return string();
      return child.substr( pos+1, string::npos );
   }

   void register_account_operation::evaluate( transaction_evaluation_state& eval_state )
   { try {
      auto now = eval_state._current_state->now();

      if( !eval_state._current_state->is_valid_account_name( this->name ) )
         FC_CAPTURE_AND_THROW( invalid_account_name, (name) );

      auto current_account = eval_state._current_state->get_account_record( this->name );
      if( current_account ) 
      {
         if( fc::time_point(current_account->last_update) + one_year < fc::time_point(now) )
            FC_CAPTURE_AND_THROW( account_already_registered, (name) );  
      }

      string parent_name = get_parent_account_name( this->name );
      if( parent_name.size() )
      {
         auto parent_record = eval_state._current_state->get_account_record( parent_name );
         if( !parent_record ) FC_CAPTURE_AND_THROW( unknown_account_name, (parent_name) );
         if( !eval_state.check_signature( parent_record->active_key() ) )
         {
            FC_CAPTURE_AND_THROW( missing_parent_account_signature, (parent_name) );
         }
         if( parent_record->is_retracted() )
            FC_CAPTURE_AND_THROW( parent_account_retracted, (parent_record) );
      }


      auto account_with_same_key = eval_state._current_state->get_account_record( address(this->active_key) );
      if( account_with_same_key.valid() )
         FC_CAPTURE_AND_THROW( account_key_in_use, (active_key)(account_with_same_key) );

      account_record new_record;
      new_record.id                = eval_state._current_state->new_account_id();
      new_record.name              = this->name;
      new_record.public_data       = this->public_data;
      new_record.owner_key         = this->owner_key;
      new_record.last_update       = now;
      new_record.registration_date = now;

      new_record.set_active_key( now, this->active_key );
      if (this->is_delegate)
      {
          new_record.delegate_info = delegate_stats();
          eval_state.required_fees += asset(eval_state._current_state->get_delegate_registration_fee(),0);
      }
      new_record.meta_data = this->meta_data;

      eval_state._current_state->store_account_record( new_record );
   } FC_CAPTURE_AND_RETHROW( (*this) ) }


   void withdraw_pay_operation::evaluate( transaction_evaluation_state& eval_state )
   { try {
      if( this->amount <= 0 ) FC_CAPTURE_AND_THROW( negative_withdraw, (amount) );

      auto pay_to_account_id = abs(this->account_id);
      auto pay_to_account = eval_state._current_state->get_account_record( pay_to_account_id ); 
      if( !pay_to_account ) FC_CAPTURE_AND_THROW( unknown_account_id, (pay_to_account_id) );
      if( pay_to_account->is_retracted() ) FC_CAPTURE_AND_THROW( account_retracted, (pay_to_account) );
      if( !pay_to_account->is_delegate() ) FC_CAPTURE_AND_THROW( not_a_delegate, (pay_to_account) );

      auto active_key = pay_to_account->active_key();
      if( !eval_state.check_signature( active_key ) )
         FC_CAPTURE_AND_THROW( missing_signature, (active_key) );

      eval_state.sub_vote( pay_to_account_id, this->amount );

      if( pay_to_account->delegate_info->pay_balance < this->amount )
         FC_CAPTURE_AND_THROW( insufficient_funds, (pay_to_account)(amount) );

      pay_to_account->delegate_info->pay_balance -= this->amount;

      eval_state._current_state->store_account_record( *pay_to_account );
      eval_state.add_balance( asset(this->amount, 0) );

   } FC_CAPTURE_AND_RETHROW( (*this) ) }


   void update_account_operation::evaluate( transaction_evaluation_state& eval_state )
   { try {
      auto current_record = eval_state._current_state->get_account_record( this->account_id );
      if( !current_record ) FC_CAPTURE_AND_THROW( unknown_account_id, (account_id) );
      if( current_record->is_retracted() ) FC_CAPTURE_AND_THROW( account_retracted, (current_record) );

      string parent_name = get_parent_account_name( current_record->name );
      if( this->active_key && *this->active_key != current_record->active_key() )
      {
         // check signature of any parent record...
         if( parent_name.size() == 0 )
         {
            if( !eval_state.check_signature(current_record->owner_key)  )
               FC_CAPTURE_AND_THROW( missing_signature, (current_record->owner_key) );
         }
         else
         {
             auto parent_record = eval_state._current_state->get_account_record( parent_name );
             if( !parent_record )
                FC_CAPTURE_AND_THROW( unknown_parent_account_name, (parent_name) );

             bool verified = false;
             while( parent_record.valid() )
             {
                if( parent_record->is_retracted() )
                   FC_CAPTURE_AND_THROW( parent_account_retracted, (parent_name) );

                if( eval_state.check_signature( parent_record->owner_key ) || 
                    eval_state.check_signature( parent_record->active_address() ) )
                   verified = true;
                else
                {
                   parent_name = get_parent_account_name( parent_name );
                   parent_record = eval_state._current_state->get_account_record( parent_name );
                }
             }
             if( !verified )
             {
                string message = "updating the active key for this account requires the signature "
                                 "of the owner or one of their parent accounts";
                FC_CAPTURE_AND_THROW( missing_signature, (message) );
             }
         }
      }
      else
      {
         bool verified = eval_state.check_signature( current_record->active_address() );
         if( !verified ) verified = eval_state.check_signature( current_record->owner_key );

         if( !verified && parent_name.size() )
         {
             auto parent_record = eval_state._current_state->get_account_record( parent_name );
             while( parent_record.valid() )
             {
                if( parent_record->is_retracted() )
                   FC_CAPTURE_AND_THROW( parent_account_retracted, (parent_name) );

                if( eval_state.check_signature( parent_record->owner_key ) || 
                    eval_state.check_signature( parent_record->active_address() ) )
                   verified = true;
                else
                {
                   parent_name = get_parent_account_name( parent_name );
                   parent_record = eval_state._current_state->get_account_record( parent_name );
                }
             }
         }
         if( !verified )
         {
            string message = "updating the active key for this account requires the signature "
                             "of the owner or one of their parent accounts";
            FC_CAPTURE_AND_THROW( missing_signature, (message) );
         }
      }

      if( this->public_data.valid() )
      {
         current_record->public_data  = *this->public_data;
      }

      current_record->last_update   = eval_state._current_state->now();

      if( this->active_key.valid() )
      {
         current_record->set_active_key( eval_state._current_state->now(), *this->active_key );
         auto account_with_same_key = eval_state._current_state->get_account_record( address(*this->active_key) );
         if( account_with_same_key )
            FC_CAPTURE_AND_THROW( account_key_in_use, (active_key)(account_with_same_key) );
      }

      if ( !current_record->is_delegate() && this->is_delegate )
      {
         // pay fee
         eval_state.required_fees += asset(eval_state._current_state->get_delegate_registration_fee(),0);
         current_record->delegate_info = delegate_stats();
      }

      eval_state._current_state->store_account_record( *current_record );
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

} } // bts::blockchain
