#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/fork_blocks.hpp>
#include <bts/blockchain/market_engine.hpp>
#include <bts/blockchain/market_operations.hpp>

namespace bts { namespace blockchain {

void bid_operation::evaluate_v1( transaction_evaluation_state& eval_state )const
{ try {
   if( this->bid_index.order_price == price() )
      FC_CAPTURE_AND_THROW( zero_price, (bid_index.order_price) );

   auto owner = this->bid_index.owner;

   auto base_asset_rec = eval_state.pending_state()->get_asset_record( bid_index.order_price.base_asset_id );
   auto quote_asset_rec = eval_state.pending_state()->get_asset_record( bid_index.order_price.quote_asset_id );
   FC_ASSERT( base_asset_rec.valid() );
   FC_ASSERT( quote_asset_rec.valid() );

   const bool authority_is_retracting = quote_asset_rec->flag_is_active( asset_record::retractable_balances )
                                        && eval_state.verify_authority( quote_asset_rec->authority );

   if( !authority_is_retracting && !eval_state.check_signature( owner ) )
      FC_CAPTURE_AND_THROW( missing_signature, (bid_index.owner) );

   asset delta_amount = this->get_amount();

   auto current_bid = eval_state.pending_state()->get_bid_record( this->bid_index );

   if( this->amount == 0 ) FC_CAPTURE_AND_THROW( zero_amount );
   if( this->amount <  0 ) // withdraw
   {
       if( NOT current_bid )
          FC_CAPTURE_AND_THROW( unknown_market_order, (bid_index) );

       if( llabs(this->amount) > current_bid->balance )
          FC_CAPTURE_AND_THROW( insufficient_funds, (amount)(current_bid->balance) );

       // add the delta amount to the eval state that we withdrew from the bid
       eval_state.add_balance( -delta_amount );
   }
   else // this->amount > 0 - deposit
   {
       if( NOT current_bid )  // then initialize to 0
         current_bid = order_record();
       // sub the delta amount from the eval state that we deposited to the bid
       eval_state.sub_balance( delta_amount );
   }

   current_bid->last_update = eval_state.pending_state()->now();
   current_bid->balance     += this->amount;

   eval_state.pending_state()->store_bid_record( this->bid_index, *current_bid );

   //auto check   = eval_state.pending_state()->get_bid_record( this->bid_index );
} FC_CAPTURE_AND_RETHROW( (*this) ) }

void ask_operation::evaluate_v1( transaction_evaluation_state& eval_state )const
{ try {
   if( this->ask_index.order_price == price() )
      FC_CAPTURE_AND_THROW( zero_price, (ask_index.order_price) );

   auto owner = this->ask_index.owner;
   if( !eval_state.check_signature( owner ) )
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

   auto market_stat = eval_state.pending_state()->get_status_record( status_index{ ask_index.order_price.quote_asset_id, ask_index.order_price.base_asset_id } );

   if( !market_stat )
      market_stat = status_record( ask_index.order_price.quote_asset_id, ask_index.order_price.base_asset_id );
   market_stat->ask_depth += delta_amount.amount;

   eval_state.pending_state()->store_status_record( *market_stat );

   eval_state.pending_state()->store_ask_record( this->ask_index, *current_ask );

   //auto check   = eval_state.pending_state()->get_ask_record( this->ask_index );
} FC_CAPTURE_AND_RETHROW( (*this) ) }

void short_operation::evaluate_v1( transaction_evaluation_state& eval_state )const
{
   if( this->short_index.order_price == price() )
      FC_CAPTURE_AND_THROW( zero_price, (short_index.order_price) );

   auto owner = this->short_index.owner;

   asset delta_amount  = this->get_amount();
   asset delta_quote   = delta_amount * this->short_index.order_price;

   // Only allow using BTSX as collateral
   FC_ASSERT( delta_amount.asset_id == 0 );

   /** if the USD amount of the order is effectively then don't bother */
   FC_ASSERT( llabs( delta_quote.amount ) > 0, "", ("delta_quote",delta_quote)("order",*this));

   auto  asset_to_short = eval_state.pending_state()->get_asset_record( short_index.order_price.quote_asset_id );
   FC_ASSERT( asset_to_short.valid() );
   FC_ASSERT( asset_to_short->is_market_issued(), "${symbol} is not a market issued asset", ("symbol",asset_to_short->symbol) );

   if( eval_state.pending_state()->get_head_block_num() >= BTS_V0_4_17_FORK_BLOCK_NUM )
   {
       FC_ASSERT( !this->short_index.limit_price || *(this->short_index.limit_price) >= this->short_index.order_price, "Insufficient collateral at price limit" );
   }

   auto current_short   = eval_state.pending_state()->get_short_record( this->short_index );

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

   auto market_stat = eval_state.pending_state()->get_status_record( status_index{ short_index.order_price.quote_asset_id, short_index.order_price.base_asset_id } );
   if( !market_stat )
      market_stat = status_record( short_index.order_price.quote_asset_id, short_index.order_price.base_asset_id );

   market_stat->bid_depth += delta_amount.amount;

   eval_state.pending_state()->store_status_record( *market_stat );

   eval_state.pending_state()->store_short_record( this->short_index, *current_short );
}

void add_collateral_operation::evaluate_v1( transaction_evaluation_state& eval_state )const
{
   if( this->cover_index.order_price == price() )
      FC_CAPTURE_AND_THROW( zero_price, (cover_index.order_price) );

   if( this->amount == 0 )
      FC_CAPTURE_AND_THROW( zero_amount );

   if( this->amount < 0 )
      FC_CAPTURE_AND_THROW( negative_deposit );

   asset delta_amount  = this->get_amount();
   eval_state.sub_balance( delta_amount );

   // update collateral and call price
   auto current_cover   = eval_state.pending_state()->get_collateral_record( this->cover_index );
   if( NOT current_cover )
      FC_CAPTURE_AND_THROW( unknown_market_order, (cover_index) );

   current_cover->collateral_balance += delta_amount.amount;

   // changing the payoff balance changes the call price... so we need to remove the old record
   // and insert a new one.
   eval_state.pending_state()->store_collateral_record( this->cover_index, collateral_record() );

   auto new_call_price = asset( current_cover->payoff_balance, cover_index.order_price.quote_asset_id ) /
                         asset( (current_cover->collateral_balance*2)/3, cover_index.order_price.base_asset_id );

   eval_state.pending_state()->store_collateral_record( market_index_key( new_call_price, this->cover_index.owner),
                                                       *current_cover );

   auto market_stat = eval_state.pending_state()->get_status_record( status_index{ cover_index.order_price.quote_asset_id, cover_index.order_price.base_asset_id } );
   FC_ASSERT( market_stat, "this should be valid for there to even be a position to cover" );
   market_stat->ask_depth += delta_amount.amount;

   eval_state.pending_state()->store_status_record( *market_stat );
}

void short_operation_v1::evaluate( transaction_evaluation_state& eval_state )const
{
   if( eval_state.pending_state()->get_head_block_num() >= BTS_V0_4_16_FORK_BLOCK_NUM )
   {
      FC_ASSERT( !"short_operation_v1 is no longer supported!" );
   }
   else if( eval_state.pending_state()->get_head_block_num() < BTS_V0_4_12_FORK_BLOCK_NUM )
   {
      evaluate_v1( eval_state );
      return;
   }

   if( this->short_index.order_price == price() )
      FC_CAPTURE_AND_THROW( zero_price, (short_index.order_price) );

   auto owner = this->short_index.owner;
   if( !eval_state.check_signature( owner ) )
      FC_CAPTURE_AND_THROW( missing_signature, (short_index.owner) );

   asset delta_amount  = this->get_amount();
   asset delta_quote   = delta_amount * this->short_index.order_price;

   /** if the USD amount of the order is effectively then don't bother */
   FC_ASSERT( llabs(delta_quote.amount) > 0, "", ("delta_quote",delta_quote)("order",*this));

   auto  asset_to_short = eval_state.pending_state()->get_asset_record( short_index.order_price.quote_asset_id );
   FC_ASSERT( asset_to_short.valid() );
   FC_ASSERT( asset_to_short->is_market_issued(), "${symbol} is not a market issued asset", ("symbol",asset_to_short->symbol) );

   auto current_short   = eval_state.pending_state()->get_short_record( this->short_index );

   if( this->amount == 0 ) FC_CAPTURE_AND_THROW( zero_amount );
   if( this->amount <  0 ) // withdraw
   {
       if( NOT current_short )
          FC_CAPTURE_AND_THROW( unknown_market_order, (short_index) );

       if( llabs(this->amount) > current_short->balance )
          FC_CAPTURE_AND_THROW( insufficient_funds, (amount)(current_short->balance) );

       // add the delta amount to the eval state that we withdrew from the short
       eval_state.add_balance( -delta_amount );
   }
   else // this->amount > 0 - deposit
   {
       if( NOT current_short )  // then initialize to 0
         current_short = order_record();
       // sub the delta amount from the eval state that we deposited to the short
       eval_state.sub_balance( delta_amount );
   }

   current_short->balance     += this->amount;
   FC_ASSERT( current_short->balance >= 0 );

   auto market_stat = eval_state.pending_state()->get_status_record( status_index{ short_index.order_price.quote_asset_id, short_index.order_price.base_asset_id } );
   if( !market_stat )
      market_stat = status_record( short_index.order_price.quote_asset_id, short_index.order_price.base_asset_id );

   if( amount > 0 )
   {
      /**
       *  If there is an average, then keep it within 10% of the average.
       */
      if( market_stat->center_price.quote_asset_id != 0 )
      {
         FC_ASSERT( short_index.order_price < market_stat->maximum_bid(), "", ("order",*this)("market_stat",market_stat) );
      }
      else // if there is no average, there must be a median feed and the short must not be more than 10% above the feed
      {
         auto median_delegate_price = eval_state.pending_state()->get_active_feed_price( short_index.order_price.quote_asset_id );
         FC_ASSERT( median_delegate_price.valid() );
         auto feed_max_short_bid = *median_delegate_price;
         feed_max_short_bid.ratio *= 10;
         feed_max_short_bid.ratio /= 9;
         FC_ASSERT( short_index.order_price < feed_max_short_bid, "", ("order",*this)("max_short_price",feed_max_short_bid) );
      }
   }

   market_stat->bid_depth += delta_amount.amount;

   eval_state.pending_state()->store_status_record( *market_stat );

   eval_state.pending_state()->store_short_record( this->short_index, *current_short );
}

void short_operation_v1::evaluate_v1( transaction_evaluation_state& eval_state )const
{
   if( this->short_index.order_price == price() )
      FC_CAPTURE_AND_THROW( zero_price, (short_index.order_price) );

   auto owner = this->short_index.owner;
   if( !eval_state.check_signature( owner ) )
      FC_CAPTURE_AND_THROW( missing_signature, (short_index.owner) );

   asset delta_amount  = this->get_amount();
   asset delta_quote   = delta_amount * this->short_index.order_price;

   /** if the USD amount of the order is effectively then don't bother */
   FC_ASSERT( llabs(delta_quote.amount) > 0, "", ("delta_quote",delta_quote)("order",*this));

   auto asset_to_short = eval_state.pending_state()->get_asset_record( short_index.order_price.quote_asset_id );
   FC_ASSERT( asset_to_short.valid() );
   FC_ASSERT( asset_to_short->is_market_issued(), "${symbol} is not a market issued asset", ("symbol",asset_to_short->symbol) );

   auto market_stat = eval_state.pending_state()->get_status_record( status_index{ short_index.order_price.quote_asset_id, short_index.order_price.base_asset_id } );
   if( !market_stat )
      market_stat = status_record( short_index.order_price.quote_asset_id, short_index.order_price.base_asset_id );

   if( market_stat->center_price.quote_asset_id != 0 )
   {
      FC_ASSERT( short_index.order_price < market_stat->maximum_bid(), "", ("order",*this)("market_stat",market_stat) );
   }
   else if( eval_state.pending_state()->get_head_block_num() >= BTS_V0_4_0_FORK_BLOCK_NUM )
   {
      auto median_delegate_price = eval_state.pending_state()->get_active_feed_price( short_index.order_price.quote_asset_id );
      FC_ASSERT( median_delegate_price.valid() );
      auto feed_max_short_bid = *median_delegate_price;
      feed_max_short_bid.ratio *= 4;
      feed_max_short_bid.ratio /= 3;
      FC_ASSERT( short_index.order_price < feed_max_short_bid, "", ("order",*this)("max_short_price",feed_max_short_bid) );
   }
   /*
   if( this->short_index.order_price > asset_to_short->maximum_xts_price ||
       this->short_index.order_price < asset_to_short->minimum_xts_price )
   {
      FC_CAPTURE_AND_THROW( price_out_of_range, (asset_to_short)(short_index.order_price) );
   }
   */

   auto current_short   = eval_state.pending_state()->get_short_record( this->short_index );

   if( this->amount == 0 ) FC_CAPTURE_AND_THROW( zero_amount );
   if( this->amount <  0 ) // withdraw
   {
       if( NOT current_short )
          FC_CAPTURE_AND_THROW( unknown_market_order, (short_index) );

       if( llabs(this->amount) > current_short->balance )
          FC_CAPTURE_AND_THROW( insufficient_funds, (amount)(current_short->balance) );

       // add the delta amount to the eval state that we withdrew from the short
       eval_state.add_balance( -delta_amount );
   }
   else // this->amount > 0 - deposit
   {
       if( NOT current_short )  // then initialize to 0
         current_short = order_record();
       // sub the delta amount from the eval state that we deposited to the short
       eval_state.sub_balance( delta_amount );
   }

   current_short->balance     += this->amount;
   FC_ASSERT( current_short->balance >= 0 );

   market_stat->bid_depth += delta_amount.amount;

   eval_state.pending_state()->store_status_record( *market_stat );

   eval_state.pending_state()->store_short_record( this->short_index, *current_short );

   //auto check   = eval_state.pending_state()->get_ask_record( this->ask_index );
}

void cover_operation::evaluate_v1( transaction_evaluation_state& eval_state )const
{
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
                            asset((current_cover->collateral_balance*3)/4, 0);

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

} }  // bts::blockchain
