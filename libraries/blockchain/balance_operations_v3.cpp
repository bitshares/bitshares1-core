#include <bts/blockchain/balance_operations.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/fork_blocks.hpp>
#include <bts/blockchain/pending_chain_state.hpp>

namespace bts { namespace blockchain {

void withdraw_operation::evaluate_v3( transaction_evaluation_state& eval_state )const
{ try {
   if( eval_state.pending_state()->get_head_block_num() < BTS_V0_4_15_FORK_BLOCK_NUM )
      return evaluate_v2( eval_state );

    if( this->amount <= 0 )
       FC_CAPTURE_AND_THROW( negative_deposit, (amount) );

   obalance_record current_balance_record = eval_state.pending_state()->get_balance_record( this->balance_id );

   if( !current_balance_record )
      FC_CAPTURE_AND_THROW( unknown_balance_record, (balance_id) );

   if( this->amount > current_balance_record->balance ) // Some withdraw conditions require extra checks (e.g. vesting condition)
      FC_CAPTURE_AND_THROW( insufficient_funds,
                            (current_balance_record)
                            (amount)
                            (current_balance_record->balance - amount) );

   switch( (withdraw_condition_types)current_balance_record->condition.type )
   {
      case withdraw_signature_type:
      {
         auto owner = current_balance_record->condition.as<withdraw_with_signature>().owner;
         if( !eval_state.check_signature( owner ) )
             FC_CAPTURE_AND_THROW( missing_signature, (owner) );
         break;
      }

      default:
         FC_CAPTURE_AND_THROW( invalid_withdraw_condition, (current_balance_record->condition) );
   }
   // update delegate vote on withdrawn account..

   if( current_balance_record->condition.asset_id == 0 && current_balance_record->condition.slate_id )
      eval_state.adjust_vote( current_balance_record->condition.slate_id, -this->amount );

   auto asset_rec = eval_state.pending_state()->get_asset_record( current_balance_record->condition.asset_id );
   FC_ASSERT( asset_rec.valid() );
   if( asset_rec->is_market_issued() )
   {
      auto yield = current_balance_record->calculate_yield_v1( eval_state.pending_state()->now(),
                                                               current_balance_record->balance,
                                                               asset_rec->collected_fees,
                                                               asset_rec->current_supply );

      if( yield.amount > 0 )
      {
         asset_rec->collected_fees       -= yield.amount;
         current_balance_record->balance += yield.amount;
         current_balance_record->deposit_date = eval_state.pending_state()->now();
         eval_state.yield_claimed[current_balance_record->condition.asset_id] += yield.amount;
         eval_state.pending_state()->store_asset_record( *asset_rec );
      }
   }

   current_balance_record->balance -= this->amount;
   current_balance_record->last_update = eval_state.pending_state()->now();

   eval_state.pending_state()->store_balance_record( *current_balance_record );
   eval_state.add_balance( asset(this->amount, current_balance_record->condition.asset_id) );
} FC_CAPTURE_AND_RETHROW( (*this) ) }

} }  // bts::blockchain
