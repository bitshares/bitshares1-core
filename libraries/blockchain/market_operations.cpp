#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/market_operations.hpp>

namespace bts { namespace blockchain {

   /**
    *  If the amount is negative then it will withdraw/cancel the bid assuming
    *  it is signed by the owner and there is sufficient funds.
    *
    *  If the amount is positive then it will add funds to the bid.
    */
   void bid_operation::evaluate( transaction_evaluation_state& eval_state )
   { try {
      if( this->bid_index.order_price == price() )
         FC_CAPTURE_AND_THROW( zero_price, (bid_index.order_price) );

      auto owner = this->bid_index.owner;
      if( !eval_state.check_signature( owner ) )
         FC_CAPTURE_AND_THROW( missing_signature, (bid_index.owner) );

      asset delta_amount  = this->get_amount();

      eval_state.validate_asset( delta_amount );

      auto current_bid   = eval_state._current_state->get_bid_record( this->bid_index );

      if( this->amount == 0 ) FC_CAPTURE_AND_THROW( zero_amount );
      if( this->amount <  0 ) // withdraw
      {
          if( NOT current_bid ) 
             FC_CAPTURE_AND_THROW( unknown_market_order, (bid_index) );

          if( abs(this->amount) > current_bid->balance )
             FC_CAPTURE_AND_THROW( insufficient_funds, (amount)(current_bid->balance) );

          // add the delta amount to the eval state that we withdrew from the bid
          eval_state.add_balance( -delta_amount );
      }
      else // this->amount > 0 - deposit
      {
          if( NOT current_bid )  // then initialize to 0
            current_bid = order_record();
          // sub the delta amount from the eval state that we deposited to the bid
          eval_state.sub_balance( balance_id_type(), delta_amount );
      }

      current_bid->balance     += this->amount;

      // bids do not count toward depth... they can set any price they like and create arbitrary depth
      //auto market_stat = eval_state._current_state->get_market_status( bid_index.order_price.quote_asset_id, bid_index.order_price.base_asset_id );
      //if( !market_stat )
      //   market_stat = market_status(0,0);
      // market_stat->bid_depth += (delta_amount * bid_index.order_price).amount;
      //eval_state._current_state->store_market_status( *market_stat );

      eval_state._current_state->store_bid_record( this->bid_index, *current_bid );

      //auto check   = eval_state._current_state->get_bid_record( this->bid_index );
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

   /**
    *  If the amount is negative then it will withdraw/cancel the bid assuming
    *  it is signed by the owner and there is sufficient funds.
    *
    *  If the amount is positive then it will add funds to the bid.
    */
   void ask_operation::evaluate( transaction_evaluation_state& eval_state )
   { try {
      if( this->ask_index.order_price == price() )
         FC_CAPTURE_AND_THROW( zero_price, (ask_index.order_price) );

      auto owner = this->ask_index.owner;
      if( !eval_state.check_signature( owner ) )
         FC_CAPTURE_AND_THROW( missing_signature, (ask_index.owner) );

      asset delta_amount  = this->get_amount();

      eval_state.validate_asset( delta_amount );

      auto current_ask   = eval_state._current_state->get_ask_record( this->ask_index );


      if( this->amount == 0 ) FC_CAPTURE_AND_THROW( zero_amount );
      if( this->amount <  0 ) // withdraw
      {
          if( NOT current_ask ) 
             FC_CAPTURE_AND_THROW( unknown_market_order, (ask_index) );

          if( abs(this->amount) > current_ask->balance )
             FC_CAPTURE_AND_THROW( insufficient_funds, (amount)(current_ask->balance) );

          // add the delta amount to the eval state that we withdrew from the ask
          eval_state.add_balance( -delta_amount );
      }
      else // this->amount > 0 - deposit
      {
          if( NOT current_ask )  // then initialize to 0
            current_ask = order_record();
          // sub the delta amount from the eval state that we deposited to the ask
          eval_state.sub_balance( balance_id_type(), delta_amount );
      }
      
      current_ask->balance     += this->amount;

      auto market_stat = eval_state._current_state->get_market_status( ask_index.order_price.quote_asset_id, ask_index.order_price.base_asset_id );

      if( !market_stat )
         market_stat = market_status(ask_index.order_price.quote_asset_id, ask_index.order_price.base_asset_id, 0,0);
      market_stat->ask_depth += delta_amount.amount;

      eval_state._current_state->store_market_status( *market_stat );

      eval_state._current_state->store_ask_record( this->ask_index, *current_ask );

      //auto check   = eval_state._current_state->get_ask_record( this->ask_index );
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

   void short_operation::evaluate( transaction_evaluation_state& eval_state )
   {
      if( this->short_index.order_price == price() )
         FC_CAPTURE_AND_THROW( zero_price, (short_index.order_price) );

      auto owner = this->short_index.owner;
      if( !eval_state.check_signature( owner ) )
         FC_CAPTURE_AND_THROW( missing_signature, (short_index.owner) );

      asset delta_amount  = this->get_amount();

      eval_state.validate_asset( delta_amount );
      auto  asset_to_short = eval_state._current_state->get_asset_record( short_index.order_price.quote_asset_id );
      FC_ASSERT( asset_to_short.valid() );
      FC_ASSERT( asset_to_short->is_market_issued(), "${symbol} is not a market issued asset", ("symbol",asset_to_short->symbol) );

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

          if( abs(this->amount) > current_short->balance )
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

      auto market_stat = eval_state._current_state->get_market_status( short_index.order_price.quote_asset_id, short_index.order_price.base_asset_id );

      if( !market_stat )
         market_stat = market_status(short_index.order_price.quote_asset_id, short_index.order_price.base_asset_id, 0,0);
      market_stat->bid_depth += delta_amount.amount;

      eval_state._current_state->store_market_status( *market_stat );

      eval_state._current_state->store_short_record( this->short_index, *current_short );

      //auto check   = eval_state._current_state->get_ask_record( this->ask_index );
   }


   /**
     pay off part of the USD balance, if balance goes to 0 then close out
     the position and transfer collateral to proper place.
     update the call price (remove old value, add new value)
   */
   void cover_operation::evaluate( transaction_evaluation_state& eval_state )
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

   void add_collateral_operation::evaluate( transaction_evaluation_state& eval_state )
   {
      if( this->cover_index.order_price == price() )
         FC_CAPTURE_AND_THROW( zero_price, (cover_index.order_price) );

      if( this->amount == 0 ) 
         FC_CAPTURE_AND_THROW( zero_amount );

      if( this->amount < 0 ) 
         FC_CAPTURE_AND_THROW( negative_deposit );

      asset delta_amount  = this->get_amount();
      eval_state.sub_balance( address(), delta_amount );

      // update collateral and call price
      auto current_cover   = eval_state._current_state->get_collateral_record( this->cover_index );
      if( NOT current_cover )
         FC_CAPTURE_AND_THROW( unknown_market_order, (cover_index) );

      current_cover->collateral_balance += delta_amount.amount;

      // changing the payoff balance changes the call price... so we need to remove the old record
      // and insert a new one.
      eval_state._current_state->store_collateral_record( this->cover_index, collateral_record() ); 

      auto new_call_price = asset(current_cover->payoff_balance, delta_amount.asset_id) /
                            asset((current_cover->collateral_balance*3)/4, 0);

      eval_state._current_state->store_collateral_record( market_index_key( new_call_price, this->cover_index.owner),
                                                          *current_cover );

      auto market_stat = eval_state._current_state->get_market_status( cover_index.order_price.quote_asset_id, cover_index.order_price.base_asset_id );
      FC_ASSERT( market_stat, "this should be valid for there to even be a position to cover" );
      market_stat->ask_depth += delta_amount.amount;

      eval_state._current_state->store_market_status( *market_stat );
   }

   void remove_collateral_operation::evaluate( transaction_evaluation_state& eval_state )
   {
      // Should this even be allowed?
   }

} } // bts::blockchain
