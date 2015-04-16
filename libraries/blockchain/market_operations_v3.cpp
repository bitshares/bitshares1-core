#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/fork_blocks.hpp>
#include <bts/blockchain/market_engine.hpp>
#include <bts/blockchain/market_operations.hpp>
#include <fc/real128.hpp>

namespace bts { namespace blockchain {

void short_operation::evaluate_v3( transaction_evaluation_state& eval_state )const
{
   if( eval_state.pending_state()->get_head_block_num() < BTS_V0_7_0_FORK_BLOCK_NUM )
      return evaluate_v2( eval_state );

   const auto base_asset_rec = eval_state.pending_state()->get_asset_record( short_index.order_price.base_asset_id );
   const auto quote_asset_rec = eval_state.pending_state()->get_asset_record( short_index.order_price.quote_asset_id );
   FC_ASSERT( base_asset_rec.valid() );
   FC_ASSERT( quote_asset_rec.valid() );

   auto owner = this->short_index.owner;
   FC_ASSERT( short_index.order_price.quote_asset_id > short_index.order_price.base_asset_id,
              "Interest rate price must have valid base and quote IDs" );

   static const fc::uint128 max_apr = fc::uint128( BTS_BLOCKCHAIN_MAX_SHORT_APR_PCT ) * FC_REAL128_PRECISION / 100;
   FC_ASSERT( short_index.order_price.ratio <= max_apr, "Interest rate too high!" );

   asset delta_amount = this->get_amount();

   // Only allow using the base asset as collateral
   FC_ASSERT( delta_amount.asset_id == 0 );

   auto  asset_to_short = eval_state.pending_state()->get_asset_record( short_index.order_price.quote_asset_id );
   FC_ASSERT( asset_to_short.valid() );
   FC_ASSERT( asset_to_short->is_market_issued(), "${symbol} is not a market issued asset", ("symbol",asset_to_short->symbol) );

   auto current_short   = eval_state.pending_state()->get_short_record( this->short_index );
   //if( current_short ) wdump( (current_short) );

   if( this->amount == 0 ) FC_CAPTURE_AND_THROW( zero_amount );
   if( this->amount <  0 ) // withdraw
   {
       if( !eval_state.check_signature( owner ) )
          FC_CAPTURE_AND_THROW( missing_signature, (short_index.owner) );

       if( NOT current_short )
          FC_CAPTURE_AND_THROW( unknown_market_order, (short_index) );

       if( llabs(this->amount) > current_short->balance )
          FC_CAPTURE_AND_THROW( insufficient_funds, (amount)(current_short->balance) );

       // add the delta amount to the eval state that we withdrew from the short
       eval_state.add_balance( -delta_amount );
   }
   else // this->amount > 0 - deposit
   {
       FC_ASSERT( this->amount >=  0 ); // 100 XTS min short order
       if( NOT current_short )  // then initialize to 0
         current_short = order_record();
       // sub the delta amount from the eval state that we deposited to the short
       eval_state.sub_balance( delta_amount );
   }
   current_short->limit_price = this->short_index.limit_price;
   current_short->last_update = eval_state.pending_state()->now();
   current_short->balance     += this->amount;
   FC_ASSERT( current_short->balance >= 0 );

   eval_state.pending_state()->store_short_record( this->short_index, *current_short );
}

void cover_operation::evaluate_v3( transaction_evaluation_state& eval_state )const
{
   if( eval_state.pending_state()->get_head_block_num() < BTS_V0_4_17_FORK_BLOCK_NUM )
      return evaluate_v2( eval_state );

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

   auto current_cover   = eval_state.pending_state()->get_collateral_record( this->cover_index );
   if( NOT current_cover )
      FC_CAPTURE_AND_THROW( unknown_market_order, (cover_index) );

   auto  asset_to_cover = eval_state.pending_state()->get_asset_record( cover_index.order_price.quote_asset_id );
   FC_ASSERT( asset_to_cover.valid() );
   asset_to_cover->current_supply -= delta_amount.amount;
   eval_state.pending_state()->store_asset_record( *asset_to_cover );

   current_cover->payoff_balance -= delta_amount.amount;
   // changing the payoff balance changes the call price... so we need to remove the old record
   // and insert a new one.
   eval_state.pending_state()->store_collateral_record( this->cover_index, collateral_record() );

   if( current_cover->payoff_balance > 0 )
   {
      auto new_call_price = asset( current_cover->payoff_balance, delta_amount.asset_id) /
                            asset( (current_cover->collateral_balance*2)/3, cover_index.order_price.base_asset_id );

      eval_state.pending_state()->store_collateral_record( market_index_key( new_call_price, this->cover_index.owner ),
                                                           *current_cover );
   }
   else // withdraw the collateral to the transaction to be deposited at owners discretion / cover fees
   {
      eval_state.add_balance( asset( current_cover->collateral_balance, cover_index.order_price.base_asset_id ) );

      auto market_stat = eval_state.pending_state()->get_status_record( status_index{ cover_index.order_price.quote_asset_id, cover_index.order_price.base_asset_id } );
      FC_ASSERT( market_stat, "this should be valid for there to even be a position to cover" );
      market_stat->ask_depth -= current_cover->collateral_balance;

      eval_state.pending_state()->store_status_record( *market_stat );
   }
}

} }  // bts::blockchain
