#include <bts/blockchain/balance_operations.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/fork_blocks.hpp>
#include <bts/blockchain/pending_chain_state.hpp>

namespace bts { namespace blockchain {

asset balance_record::calculate_yield_v1( fc::time_point_sec now, share_type amount, share_type yield_pool, share_type share_supply )const
{
   if( amount <= 0 )       return asset(0,condition.asset_id);
   if( share_supply <= 0 ) return asset(0,condition.asset_id);
   if( yield_pool <= 0 )   return asset(0,condition.asset_id);

   auto elapsed_time = (now - deposit_date);
   if(  elapsed_time > fc::seconds( BTS_BLOCKCHAIN_MIN_YIELD_PERIOD_SEC ) )
   {
      if( yield_pool > 0 && share_supply > 0 )
      {
         fc::uint128 amount_withdrawn( amount );
         amount_withdrawn *= 1000000;

         fc::uint128 current_supply( share_supply );
         fc::uint128 fee_fund( yield_pool );

         auto yield = (amount_withdrawn * fee_fund) / current_supply;

         /**
          *  If less than 1 year, the 80% of yield are scaled linearly with time and 20% scaled by time^2,
          *  this should produce a yield curve that is "80% simple interest" and 20% simulating compound
          *  interest.
          */
         if( elapsed_time < fc::seconds( BTS_BLOCKCHAIN_BLOCKS_PER_YEAR * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC ) )
         {
            auto original_yield = yield;
            // discount the yield by 80%
            yield *= 8;
            yield /= 10;

            auto delta_yield = original_yield - yield;

            // yield == amount withdrawn / total usd  *  fee fund * fraction_of_year
            yield *= elapsed_time.to_seconds();
            yield /= (BTS_BLOCKCHAIN_BLOCKS_PER_YEAR * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC);

            delta_yield *= elapsed_time.to_seconds();
            delta_yield /= (BTS_BLOCKCHAIN_BLOCKS_PER_YEAR * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC);

            delta_yield *= elapsed_time.to_seconds();
            delta_yield /= (BTS_BLOCKCHAIN_BLOCKS_PER_YEAR * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC);

            yield += delta_yield;
         }

         yield /= 1000000;
         auto yield_amount = yield.to_uint64();

         if( yield_amount > 0 && yield_amount < yield_pool )
         {
            return asset( yield_amount, condition.asset_id );
         }
      }
   }
   return asset( 0, condition.asset_id );
}

void deposit_operation::evaluate_v1( transaction_evaluation_state& eval_state )const
{ try {
    if( this->amount <= 0 )
       FC_CAPTURE_AND_THROW( negative_deposit, (amount) );

    switch( withdraw_condition_types( this->condition.type ) )
    {
       case withdraw_signature_type:
          break;
       default:
          FC_CAPTURE_AND_THROW( invalid_withdraw_condition, (*this) );
    }

    auto deposit_balance_id = this->balance_id();

    auto cur_record = eval_state.pending_state()->get_balance_record( deposit_balance_id );
    if( !cur_record )
    {
       cur_record = balance_record( this->condition );
    }
    cur_record->last_update   = eval_state.pending_state()->now();
    cur_record->balance       += this->amount;

    eval_state.sub_balance( asset(this->amount, cur_record->condition.asset_id) );

    if( cur_record->condition.asset_id == 0 && cur_record->condition.slate_id )
       eval_state.adjust_vote( cur_record->condition.slate_id, this->amount );

    eval_state.pending_state()->store_balance_record( *cur_record );
} FC_CAPTURE_AND_RETHROW( (*this) ) }

void withdraw_operation::evaluate_v1( transaction_evaluation_state& eval_state )const
{ try {
    if( this->amount <= 0 )
       FC_CAPTURE_AND_THROW( negative_deposit, (amount) );

   obalance_record current_balance_record = eval_state.pending_state()->get_balance_record( this->balance_id );

   if( eval_state.pending_state()->get_head_block_num() >= BTS_V0_4_10_FORK_BLOCK_NUM )
   {
      if( !current_balance_record )
         FC_CAPTURE_AND_THROW( unknown_balance_record, (balance_id) );

      if( this->amount > current_balance_record->balance )
         FC_CAPTURE_AND_THROW( insufficient_funds,
                               (current_balance_record)
                               (amount)
                               (current_balance_record->balance - amount) );
   }

   if( current_balance_record )
   {
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

      eval_state.pending_state()->store_balance_record( *current_balance_record );
      eval_state.add_balance( asset(this->amount, current_balance_record->condition.asset_id) );
   } // if current balance record
   else
   {
      eval_state.add_balance( asset(this->amount, 0) );
   }
} FC_CAPTURE_AND_RETHROW( (*this) ) }

void burn_operation::evaluate_v1( transaction_evaluation_state& eval_state )const
{ try {
   if( this->amount.amount <= 0 )
      FC_CAPTURE_AND_THROW( negative_deposit, (amount) );

   if( !message.empty() )
       FC_ASSERT( amount.asset_id == 0 );

   if( amount.asset_id == 0 )
   {
       FC_ASSERT( amount.amount >= BTS_BLOCKCHAIN_MIN_BURN_FEE );
   }

   oasset_record asset_rec = eval_state.pending_state()->get_asset_record( amount.asset_id );
   FC_ASSERT( asset_rec.valid() );
   FC_ASSERT( !asset_rec->is_market_issued() );

   asset_rec->current_supply -= this->amount.amount;
   eval_state.sub_balance( this->amount );

   eval_state.pending_state()->store_asset_record( *asset_rec );

   if( account_id != 0 ) // you can offer burnt offerings to God if you like... otherwise it must be an account
   {
       const oaccount_record account_rec = eval_state.pending_state()->get_account_record( abs( this->account_id ) );
       FC_ASSERT( account_rec.valid() );
   }

   burn_record record;
   record.index.account_id = account_id;
   record.index.transaction_id = eval_state.trx.id();
   record.amount = amount;
   record.message = message;
   record.signer = message_signature;

   FC_ASSERT( !eval_state.pending_state()->get_burn_record( record.index ).valid() );

   eval_state.pending_state()->store_burn_record( std::move( record ) );
} FC_CAPTURE_AND_RETHROW( (*this) ) }

} }  // bts::blockchain
