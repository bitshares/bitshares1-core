void short_operation::evaluate_v1( transaction_evaluation_state& eval_state )
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

   if( market_stat->avg_price_1h.quote_asset_id != 0 )
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
