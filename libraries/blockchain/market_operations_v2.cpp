void cover_operation::evaluate_v2( transaction_evaluation_state& eval_state )
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
                            asset((current_cover->collateral_balance*2)/3, 0);

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
