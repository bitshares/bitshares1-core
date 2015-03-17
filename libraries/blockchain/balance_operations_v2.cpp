#include <bts/blockchain/balance_operations.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/fork_blocks.hpp>
#include <bts/blockchain/pending_chain_state.hpp>

namespace bts { namespace blockchain {

void withdraw_operation::evaluate_v2( transaction_evaluation_state& eval_state )const
{ try {
   if( eval_state.pending_state()->get_head_block_num() < BTS_V0_4_13_FORK_BLOCK_NUM )
      return evaluate_v1( eval_state );

   if( this->amount <= 0 )
      FC_CAPTURE_AND_THROW( negative_deposit, (amount) );

   obalance_record current_balance_record = eval_state.pending_state()->get_balance_record( this->balance_id );

   if( !current_balance_record )
      FC_CAPTURE_AND_THROW( unknown_balance_record, (balance_id) );

   if( this->amount > current_balance_record->balance )
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

   current_balance_record->balance -= this->amount;
   current_balance_record->last_update = eval_state.pending_state()->now();

   if( current_balance_record->condition.asset_id == 0 && current_balance_record->condition.slate_id )
      eval_state.adjust_vote( current_balance_record->condition.slate_id, -this->amount );

   auto asset_rec = eval_state.pending_state()->get_asset_record( current_balance_record->condition.asset_id );
   FC_ASSERT( asset_rec.valid() );
   if( asset_rec->is_market_issued() )
   {
      auto yield = current_balance_record->calculate_yield( eval_state.pending_state()->now(),
                                                            current_balance_record->balance,
                                                            asset_rec->collected_fees,
                                                            asset_rec->current_supply );

      if( eval_state.pending_state()->get_head_block_num() < BTS_V0_4_21_FORK_BLOCK_NUM )
      {
         yield = current_balance_record->calculate_yield_v1( eval_state.pending_state()->now(),
                                                             current_balance_record->balance,
                                                             asset_rec->collected_fees,
                                                             asset_rec->current_supply );
      }

      if( yield.amount > 0 )
      {
         asset_rec->collected_fees       -= yield.amount;
         current_balance_record->balance += yield.amount;
         current_balance_record->deposit_date = eval_state.pending_state()->now();
         eval_state.yield_claimed[current_balance_record->condition.asset_id] += yield.amount;
         eval_state.pending_state()->store_asset_record( *asset_rec );
      }
   }

   eval_state.pending_state()->store_balance_record( *current_balance_record );
   eval_state.add_balance( asset(this->amount, current_balance_record->condition.asset_id) );
} FC_CAPTURE_AND_RETHROW( (*this) ) }

void deposit_operation::evaluate_v2( transaction_evaluation_state& eval_state )const
{ try {
    if( eval_state.pending_state()->get_head_block_num() < BTS_V0_4_13_FORK_BLOCK_NUM )
       return evaluate_v1( eval_state );

    if( this->amount <= 0 )
       FC_CAPTURE_AND_THROW( negative_deposit, (amount) );

    if( eval_state.pending_state()->get_head_block_num() >= BTS_V0_5_0_FORK_BLOCK_NUM )
    {
        switch( withdraw_condition_types( this->condition.type ) )
        {
           case withdraw_signature_type:
           case withdraw_multisig_type:
           case withdraw_escrow_type:
              break;
           default:
              FC_CAPTURE_AND_THROW( invalid_withdraw_condition, (*this) );
        }
    }

    const balance_id_type deposit_balance_id = this->balance_id();

    obalance_record cur_record = eval_state.pending_state()->get_balance_record( deposit_balance_id );
    if( !cur_record.valid() )
    {
       cur_record = balance_record( this->condition );
       if( this->condition.type == withdraw_escrow_type )
          cur_record->meta_data = variant_object("creating_transaction_id", eval_state.trx.id() );
    }

    if( cur_record->balance == 0 )
    {
       cur_record->deposit_date = eval_state.pending_state()->now();
    }
    else
    {
       fc::uint128 old_sec_since_epoch( cur_record->deposit_date.sec_since_epoch() );
       fc::uint128 new_sec_since_epoch( eval_state.pending_state()->now().sec_since_epoch() );

       fc::uint128 avg = (old_sec_since_epoch * cur_record->balance) + (new_sec_since_epoch * this->amount);
       avg /= (cur_record->balance + this->amount);

       cur_record->deposit_date = time_point_sec( avg.to_integer() );
    }

    cur_record->balance += this->amount;
    eval_state.sub_balance( asset( this->amount, cur_record->asset_id() ) );

    if( cur_record->condition.asset_id == 0 && cur_record->condition.slate_id )
       eval_state.adjust_vote( cur_record->condition.slate_id, this->amount );

    cur_record->last_update = eval_state.pending_state()->now();

    const oasset_record asset_rec = eval_state.pending_state()->get_asset_record( cur_record->condition.asset_id );
    FC_ASSERT( asset_rec.valid() );

    if( eval_state.pending_state()->get_head_block_num() >= BTS_V0_6_0_FORK_BLOCK_NUM )
    {
        FC_ASSERT( !eval_state.pending_state()->is_fraudulent_asset( *asset_rec ) );
    }

    if( eval_state.pending_state()->get_head_block_num() >= BTS_V0_5_0_FORK_BLOCK_NUM )
    {
        if( asset_rec->is_market_issued() )
        {
            FC_ASSERT( cur_record->condition.slate_id == 0 );
        }
    }

    eval_state.pending_state()->store_balance_record( *cur_record );
} FC_CAPTURE_AND_RETHROW( (*this) ) }

} }  // bts::blockchain
