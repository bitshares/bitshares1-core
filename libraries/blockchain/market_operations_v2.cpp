#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/fork_blocks.hpp>
#include <bts/blockchain/market_engine.hpp>
#include <bts/blockchain/market_operations.hpp>

namespace bts { namespace blockchain {

void ask_operation::evaluate_v2( transaction_evaluation_state& eval_state )const
{ try {
   if( eval_state.pending_state()->get_head_block_num() < BTS_V0_4_21_FORK_BLOCK_NUM )
      return evaluate_v1( eval_state );

   if( this->ask_index.order_price == price() )
      FC_CAPTURE_AND_THROW( zero_price, (ask_index.order_price) );

   auto owner = this->ask_index.owner;

   auto base_asset_rec = eval_state.pending_state()->get_asset_record( ask_index.order_price.base_asset_id );
   auto quote_asset_rec = eval_state.pending_state()->get_asset_record( ask_index.order_price.quote_asset_id );
   FC_ASSERT( base_asset_rec.valid() );
   FC_ASSERT( quote_asset_rec.valid() );

   const bool authority_is_retracting = base_asset_rec->flag_is_active( asset_record::retractable_balances )
                                        && eval_state.verify_authority( base_asset_rec->authority );

   if( !authority_is_retracting && !eval_state.check_signature( owner ) )
      FC_CAPTURE_AND_THROW( missing_signature, (ask_index.owner) );

   asset delta_amount  = this->get_amount();

   auto current_ask   = eval_state.pending_state()->get_ask_record( this->ask_index );

   if( this->amount == 0 ) FC_CAPTURE_AND_THROW( zero_amount );
   if( this->amount <  0 ) // withdraw
   {
       if( NOT current_ask )
          FC_CAPTURE_AND_THROW( unknown_market_order, (ask_index) );

       if( llabs(this->amount) > current_ask->balance )
          FC_CAPTURE_AND_THROW( insufficient_funds, (amount)(current_ask->balance) );

       // add the delta amount to the eval state that we withdrew from the ask
       eval_state.add_balance( -delta_amount );
   }
   else // this->amount > 0 - deposit
   {
       if( NOT current_ask )  // then initialize to 0
         current_ask = order_record();
       // sub the delta amount from the eval state that we deposited to the ask
       eval_state.sub_balance( delta_amount );
   }

   current_ask->last_update = eval_state.pending_state()->now();
   current_ask->balance     += this->amount;
   FC_ASSERT( current_ask->balance >= 0, "", ("current_ask",current_ask)  );

   eval_state.pending_state()->store_ask_record( this->ask_index, *current_ask );
} FC_CAPTURE_AND_RETHROW( (*this) ) }

void short_operation::evaluate_v2( transaction_evaluation_state& eval_state )const
{
   if( eval_state.pending_state()->get_head_block_num() < BTS_V0_4_21_FORK_BLOCK_NUM )
      return evaluate_v1( eval_state );

   const auto base_asset_rec = eval_state.pending_state()->get_asset_record( short_index.order_price.base_asset_id );
   const auto quote_asset_rec = eval_state.pending_state()->get_asset_record( short_index.order_price.quote_asset_id );
   FC_ASSERT( base_asset_rec.valid() );
   FC_ASSERT( quote_asset_rec.valid() );

   auto owner = this->short_index.owner;
   FC_ASSERT( short_index.order_price.ratio < fc::uint128( 10, 0 ) );
   FC_ASSERT( short_index.order_price.quote_asset_id > short_index.order_price.base_asset_id,
              "Interest rate price must have valid base and quote IDs" );

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

void cover_operation::evaluate_v2( transaction_evaluation_state& eval_state )const
{
   if( eval_state.pending_state()->get_head_block_num() < BTS_V0_4_16_FORK_BLOCK_NUM )
      return evaluate_v1( eval_state );

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

   current_cover->payoff_balance -= delta_amount.amount;
   // changing the payoff balance changes the call price... so we need to remove the old record
   // and insert a new one.
   eval_state.pending_state()->store_collateral_record( this->cover_index, collateral_record() );

   if( current_cover->payoff_balance > 0 )
   {
      auto new_call_price = asset(current_cover->payoff_balance, delta_amount.asset_id) /
                            asset((current_cover->collateral_balance*2)/3, 0);

      eval_state.pending_state()->store_collateral_record( market_index_key( new_call_price, this->cover_index.owner),
                                                           *current_cover );
   }
   else // withdraw the collateral to the transaction to be deposited at owners discretion / cover fees
   {
      eval_state.add_balance( asset( current_cover->collateral_balance, 0 ) );

      auto market_stat = eval_state.pending_state()->get_status_record( status_index{ cover_index.order_price.quote_asset_id, cover_index.order_price.base_asset_id } );
      FC_ASSERT( market_stat, "this should be valid for there to even be a position to cover" );
      market_stat->ask_depth -= current_cover->collateral_balance;

      eval_state.pending_state()->store_status_record( *market_stat );
   }
}

void add_collateral_operation::evaluate_v2( transaction_evaluation_state& eval_state )const
{
   if( eval_state.pending_state()->get_head_block_num() < BTS_V0_4_21_FORK_BLOCK_NUM )
      return evaluate_v1( eval_state );

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
   eval_state.sub_balance( delta_amount );

   // update collateral and call price
   auto current_cover = eval_state.pending_state()->get_collateral_record( this->cover_index );
   if( NOT current_cover )
      FC_CAPTURE_AND_THROW( unknown_market_order, (cover_index) );

   current_cover->collateral_balance += delta_amount.amount;

   const auto new_call_price = asset( current_cover->payoff_balance, cover_index.order_price.quote_asset_id )
                               / asset( (current_cover->collateral_balance*2)/3, cover_index.order_price.base_asset_id );

   // changing the payoff balance changes the call price... so we need to remove the old record
   // and insert a new one.
   eval_state.pending_state()->store_collateral_record( this->cover_index, collateral_record() );


   eval_state.pending_state()->store_collateral_record( market_index_key( new_call_price, this->cover_index.owner),
                                                       *current_cover );
}

} }  // bts::blockchain
