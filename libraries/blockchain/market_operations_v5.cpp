#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/fork_blocks.hpp>
#include <bts/blockchain/market_engine.hpp>
#include <bts/blockchain/market_engine_v7.hpp>
#include <bts/blockchain/market_operations.hpp>

namespace bts { namespace blockchain {

void cover_operation::evaluate_v5( transaction_evaluation_state& eval_state )const
{
   if( eval_state.pending_state()->get_head_block_num() < BTS_V0_4_23_FORK_BLOCK_NUM )
      return evaluate_v4( eval_state );

   const auto base_asset_rec = eval_state.pending_state()->get_asset_record( cover_index.order_price.base_asset_id );
   const auto quote_asset_rec = eval_state.pending_state()->get_asset_record( cover_index.order_price.quote_asset_id );
   FC_ASSERT( base_asset_rec.valid() );
   FC_ASSERT( quote_asset_rec.valid() );

   if( this->cover_index.order_price == price() )
      FC_CAPTURE_AND_THROW( zero_price, (cover_index.order_price) );

   if( this->amount == 0 )
      FC_CAPTURE_AND_THROW( zero_amount );

   if( this->amount < 0 )
      FC_CAPTURE_AND_THROW( negative_deposit );

   asset delta_amount  = this->get_amount();

   if( !eval_state.check_signature( cover_index.owner ) )
      FC_CAPTURE_AND_THROW( missing_signature, (cover_index.owner) );

   // subtract this from the transaction
   eval_state.sub_balance( delta_amount );

   auto current_cover = eval_state.pending_state()->get_collateral_record( this->cover_index );
   if( NOT current_cover )
      FC_CAPTURE_AND_THROW( unknown_market_order, (cover_index) );

   auto  asset_to_cover = eval_state.pending_state()->get_asset_record( cover_index.order_price.quote_asset_id );
   FC_ASSERT( asset_to_cover.valid() );

   const auto start_time = current_cover->expiration - fc::seconds( BTS_BLOCKCHAIN_MAX_SHORT_PERIOD_SEC );
   auto elapsed_sec = ( eval_state.pending_state()->now() - start_time ).to_seconds();
   if( elapsed_sec < 0 ) elapsed_sec = 0;

   const asset principle = asset( current_cover->payoff_balance, delta_amount.asset_id );
   asset total_debt = detail::market_engine::get_interest_owed( principle, current_cover->interest_rate, elapsed_sec ) + principle;

   if( eval_state.pending_state()->get_head_block_num() >= BTS_V0_7_0_FORK_BLOCK_NUM )
   {
       total_debt = detail::market_engine_v7::get_interest_owed_fixed( principle, current_cover->interest_rate, elapsed_sec ) + principle;
   }

   asset principle_paid;
   asset interest_paid;
   if( delta_amount >= total_debt )
   {
       // Payoff the whole debt
       principle_paid = principle;
       interest_paid = delta_amount - principle_paid;
       current_cover->payoff_balance = 0;
   }
   else
   {
       // Partial cover
       interest_paid = detail::market_engine::get_interest_paid( delta_amount, current_cover->interest_rate, elapsed_sec );

       if( eval_state.pending_state()->get_head_block_num() >= BTS_V0_7_0_FORK_BLOCK_NUM )
       {
           interest_paid = detail::market_engine_v7::get_interest_paid_fixed( delta_amount, current_cover->interest_rate, elapsed_sec );
       }

       principle_paid = delta_amount - interest_paid;
       current_cover->payoff_balance -= principle_paid.amount;
   }
   FC_ASSERT( principle_paid.amount >= 0 );
   FC_ASSERT( interest_paid.amount >= 0 );
   FC_ASSERT( current_cover->payoff_balance >= 0 );

   //Covered asset is destroyed, interest pays to fees
   asset_to_cover->current_supply -= principle_paid.amount;
   asset_to_cover->collected_fees += interest_paid.amount;
   eval_state.pending_state()->store_asset_record( *asset_to_cover );

   // changing the payoff balance changes the call price... so we need to remove the old record
   // and insert a new one.
   eval_state.pending_state()->store_collateral_record( this->cover_index, collateral_record() );

   FC_ASSERT( current_cover->interest_rate.quote_asset_id > current_cover->interest_rate.base_asset_id,
              "Rejecting cover order with invalid interest rate.", ("cover", *current_cover) );

   if( current_cover->payoff_balance > 0 )
   {
      const auto new_call_price = asset( current_cover->payoff_balance, delta_amount.asset_id)
                                  / asset( (current_cover->collateral_balance*2)/3, cover_index.order_price.base_asset_id );

      eval_state.pending_state()->store_collateral_record( market_index_key( new_call_price, this->cover_index.owner ),
                                                           *current_cover );
   }
   else // withdraw the collateral to the transaction to be deposited at owners discretion / cover fees
   {
      eval_state.add_balance( asset( current_cover->collateral_balance, cover_index.order_price.base_asset_id ) );
   }
}

} }  // bts::blockchain
