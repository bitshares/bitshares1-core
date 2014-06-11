#include <bts/blockchain/asset_operations.hpp>
#include <bts/blockchain/transaction_evaluation_state.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/chain_interface.hpp>

namespace bts { namespace blockchain {

 
   /**
    *  @note in this method we are using 'this->' to refer to member variables for
    *  clarity. 
    */
   void create_asset_operation::evaluate( transaction_evaluation_state& eval_state )
   { try {
      if( NOT is_power_of_ten( this->precision ) )
         FC_CAPTURE_AND_THROW( invalid_precision, (precision) );

      if( NOT eval_state._current_state->is_valid_symbol_name( this->symbol ) )
         FC_CAPTURE_AND_THROW( invalid_asset_symbol, (symbol) );

      if( this->maximum_share_supply <= 0 )
         FC_CAPTURE_AND_THROW( invalid_asset_amount, (maximum_share_supply) );

      auto current_asset_record = eval_state._current_state->get_asset_record( this->symbol );
      if( current_asset_record )
         FC_CAPTURE_AND_THROW( asset_symbol_in_use, (symbol) );

      if( issuer_account_id != asset_record::market_issued_asset )
      {
         auto issuer_account_record = eval_state._current_state->get_account_record( this->issuer_account_id );
         if( NOT issuer_account_record )
            FC_CAPTURE_AND_THROW( unknown_account_id, (issuer_account_id) );

         if( NOT eval_state.check_signature( issuer_account_record->active_address() ) )
            FC_CAPTURE_AND_THROW( missing_signature, (issuer_account_record) );
      }

      eval_state.required_fees += asset(eval_state._current_state->get_asset_registration_fee(),0);

      asset_record new_record;
      new_record.id                    = eval_state._current_state->new_asset_id();
      new_record.symbol                = this->symbol;
      new_record.name                  = this->name;
      new_record.description           = this->description;
      new_record.public_data           = this->public_data;
      new_record.issuer_account_id     = this->issuer_account_id;
      new_record.current_share_supply  = 0;
      new_record.maximum_share_supply  = this->maximum_share_supply;
      new_record.collected_fees        = 0;
      new_record.registration_date     = eval_state._current_state->now();
      new_record.precision             = this->precision;

      eval_state._current_state->store_asset_record( new_record );
      
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

   /**
    *  @note in this method we are using 'this->' to refer to member variables for
    *  clarity. 
    */
   void update_asset_operation::evaluate( transaction_evaluation_state& eval_state )
   { try {
      auto current_asset_record = eval_state._current_state->get_asset_record( this->asset_id );
      if( NOT current_asset_record )
         FC_CAPTURE_AND_THROW( unknown_asset_id, (asset_id) );

      auto issuer_account_record = eval_state._current_state->get_account_record( current_asset_record->issuer_account_id );

      if( NOT issuer_account_record )
         FC_CAPTURE_AND_THROW( unknown_account_id, (current_asset_record->issuer_account_id) );

      if( NOT eval_state.check_signature(issuer_account_record->active_address()) )
          FC_CAPTURE_AND_THROW( missing_signature, (issuer_account_record->active_key()) );

      if( this->issuer_account_id != current_asset_record->issuer_account_id )
      {
          auto new_issuer_account_record = eval_state._current_state->get_account_record( this->issuer_account_id );

          if( NOT new_issuer_account_record )
              FC_CAPTURE_AND_THROW( unknown_account_id, (issuer_account_id) );

          if( NOT eval_state.check_signature(new_issuer_account_record->active_address()) )
              FC_CAPTURE_AND_THROW( missing_signature, (new_issuer_account_record->active_key()) );
      }

      current_asset_record->description       = this->description;
      current_asset_record->public_data       = this->public_data;
      current_asset_record->issuer_account_id = this->issuer_account_id;
      current_asset_record->last_update       = eval_state._current_state->now();

      eval_state._current_state->store_asset_record( *current_asset_record );
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

   /**
    *  @note in this method we are using 'this->' to refer to member variables for
    *  clarity. 
    */
   void issue_asset_operation::evaluate( transaction_evaluation_state& eval_state )
   { try {
      if( this->amount.amount <= 0 )
         FC_CAPTURE_AND_THROW( negative_issue, (amount) );

      auto current_asset_record = eval_state._current_state->get_asset_record( this->amount.asset_id );
      if( NOT current_asset_record )
         FC_CAPTURE_AND_THROW( unknown_asset_id, (amount.asset_id) );

      auto issuer_account_record = eval_state._current_state->get_account_record( current_asset_record->issuer_account_id );
      if( NOT issuer_account_record ) 
         FC_CAPTURE_AND_THROW( unknown_account_id, (current_asset_record->issuer_account_id) );

      if( NOT eval_state.check_signature( issuer_account_record->active_address() ) ) 
      {
         FC_CAPTURE_AND_THROW( missing_signature, (issuer_account_record->active_key()) );
      }

      if( NOT current_asset_record->can_issue( this->amount ) )
      {
         FC_CAPTURE_AND_THROW( over_issue, (amount)(current_asset_record) );
      }
    
      current_asset_record->current_share_supply += this->amount.amount;
      
      eval_state.add_balance( this->amount );

      eval_state._current_state->store_asset_record( *current_asset_record );
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

} } // bts::blockchain
