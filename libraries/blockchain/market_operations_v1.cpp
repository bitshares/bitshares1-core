void short_operation_v1::evaluate( transaction_evaluation_state& eval_state )
{
   if( eval_state._current_state->get_head_block_num() >= BTSX_MARKET_FORK_7_BLOCK_NUM )
   {
      FC_ASSERT( !"short_operation_v1 is no longer supported!" );
   }
   else if( eval_state._current_state->get_head_block_num() < BTSX_MARKET_FORK_5_BLOCK_NUM )
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

   eval_state.validate_asset( delta_amount );
   auto  asset_to_short = eval_state._current_state->get_asset_record( short_index.order_price.quote_asset_id );
   FC_ASSERT( asset_to_short.valid() );
   FC_ASSERT( asset_to_short->is_market_issued(), "${symbol} is not a market issued asset", ("symbol",asset_to_short->symbol) );


   auto current_short   = eval_state._current_state->get_short_record( this->short_index );
   //if( current_short ) wdump( (current_short) );

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
       FC_ASSERT( this->amount >=  (BTS_BLOCKCHAIN_MINIMUM_SHORT_ORDER_SIZE) ); // 100 XTS min short order
       if( NOT current_short )  // then initialize to 0
         current_short = order_record();
       // sub the delta amount from the eval state that we deposited to the short
       eval_state.sub_balance( balance_id_type(), delta_amount );
   }

   current_short->balance     += this->amount;
   FC_ASSERT( current_short->balance >= 0 );

   auto market_stat = eval_state._current_state->get_market_status( short_index.order_price.quote_asset_id, short_index.order_price.base_asset_id );
   if( !market_stat )
      market_stat = market_status(short_index.order_price.quote_asset_id, short_index.order_price.base_asset_id, 0,0);

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
         auto median_delegate_price = eval_state._current_state->get_median_delegate_price( short_index.order_price.quote_asset_id );
         FC_ASSERT( median_delegate_price.valid() );
         auto feed_max_short_bid = *median_delegate_price;
         feed_max_short_bid.ratio *= 10;
         feed_max_short_bid.ratio /= 9;
         FC_ASSERT( short_index.order_price < feed_max_short_bid, "", ("order",*this)("max_short_price",feed_max_short_bid) );
      }
   }

   market_stat->bid_depth += delta_amount.amount;

   eval_state._current_state->store_market_status( *market_stat );

   eval_state._current_state->store_short_record( this->short_index, *current_short );
}

void short_operation_v1::evaluate_v1( transaction_evaluation_state& eval_state )
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

   eval_state.validate_asset( delta_amount );
   auto  asset_to_short = eval_state._current_state->get_asset_record( short_index.order_price.quote_asset_id );
   FC_ASSERT( asset_to_short.valid() );
   FC_ASSERT( asset_to_short->is_market_issued(), "${symbol} is not a market issued asset", ("symbol",asset_to_short->symbol) );

   auto market_stat = eval_state._current_state->get_market_status( short_index.order_price.quote_asset_id, short_index.order_price.base_asset_id );
   if( !market_stat )
      market_stat = market_status(short_index.order_price.quote_asset_id, short_index.order_price.base_asset_id, 0,0);

   if( market_stat->center_price.quote_asset_id != 0 )
   {
      FC_ASSERT( short_index.order_price < market_stat->maximum_bid(), "", ("order",*this)("market_stat",market_stat) );
   }
   else if( eval_state._current_state->get_head_block_num() >= BTSX_MARKET_FORK_1_BLOCK_NUM )
   {
      auto median_delegate_price = eval_state._current_state->get_median_delegate_price( short_index.order_price.quote_asset_id );
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

   auto current_short   = eval_state._current_state->get_short_record( this->short_index );
   //if( current_short ) wdump( (current_short) );

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
       eval_state.sub_balance( balance_id_type(), delta_amount );
   }

   current_short->balance     += this->amount;
   FC_ASSERT( current_short->balance >= 0 );

   market_stat->bid_depth += delta_amount.amount;

   eval_state._current_state->store_market_status( *market_stat );

   eval_state._current_state->store_short_record( this->short_index, *current_short );

   //auto check   = eval_state._current_state->get_ask_record( this->ask_index );
}

void cover_operation::evaluate_v1( transaction_evaluation_state& eval_state )
{
   if( this->cover_index.order_price == price() )
      FC_CAPTURE_AND_THROW( zero_price, (cover_index.order_price) );

   if( this->amount == 0 && !this->new_cover_price )
      FC_CAPTURE_AND_THROW( zero_amount );

   if( this->amount < 0 )
      FC_CAPTURE_AND_THROW( negative_deposit );

   asset delta_amount  = this->get_amount();

   if( !eval_state.check_signature( cover_index.owner ) )
      FC_CAPTURE_AND_THROW( missing_signature, (cover_index.owner) );

   // subtract this from the transaction
   eval_state.sub_balance( address(), delta_amount );

   auto current_cover   = eval_state._current_state->get_collateral_record( this->cover_index );
   if( NOT current_cover )
      FC_CAPTURE_AND_THROW( unknown_market_order, (cover_index) );

   current_cover->payoff_balance -= delta_amount.amount;
   // changing the payoff balance changes the call price... so we need to remove the old record
   // and insert a new one.
   eval_state._current_state->store_collateral_record( this->cover_index, collateral_record() );

   if( current_cover->payoff_balance > 0 )
   {
      auto new_call_price = asset(current_cover->payoff_balance, delta_amount.asset_id) /
                            asset((current_cover->collateral_balance*3)/4, 0);

      if( this->new_cover_price && (*this->new_cover_price > new_call_price) )
         eval_state._current_state->store_collateral_record( market_index_key( *this->new_cover_price, this->cover_index.owner),
                                                             *current_cover );
      else
         eval_state._current_state->store_collateral_record( market_index_key( new_call_price, this->cover_index.owner),
                                                             *current_cover );
   }
   else // withdraw the collateral to the transaction to be deposited at owners discretion / cover fees
   {
      eval_state.add_balance( asset( current_cover->collateral_balance, 0 ) );

      auto market_stat = eval_state._current_state->get_market_status( cover_index.order_price.quote_asset_id, cover_index.order_price.base_asset_id );
      FC_ASSERT( market_stat, "this should be valid for there to even be a position to cover" );
      market_stat->ask_depth -= current_cover->collateral_balance;

      eval_state._current_state->store_market_status( *market_stat );
   }
}
