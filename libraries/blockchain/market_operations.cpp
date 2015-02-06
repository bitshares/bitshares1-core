#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/market_engine.hpp>
#include <bts/blockchain/market_operations.hpp>
#include <bts/blockchain/pending_chain_state.hpp>

#include <algorithm>

namespace bts { namespace blockchain {

   /**
    *  If the amount is negative then it will withdraw/cancel the bid assuming
    *  it is signed by the owner and there is sufficient funds.
    *
    *  If the amount is positive then it will add funds to the bid.
    */
   void bid_operation::evaluate( transaction_evaluation_state& eval_state )const
   { try {
      if( this->bid_index.order_price == price() )
         FC_CAPTURE_AND_THROW( zero_price, (bid_index.order_price) );

      auto owner = this->bid_index.owner;

      auto base_asset_rec = eval_state._current_state->get_asset_record( bid_index.order_price.base_asset_id );
      auto quote_asset_rec = eval_state._current_state->get_asset_record( bid_index.order_price.quote_asset_id );
      FC_ASSERT( base_asset_rec.valid() );
      FC_ASSERT( quote_asset_rec.valid() );
      if( base_asset_rec->is_restricted() )
         FC_ASSERT( eval_state._current_state->get_authorization( base_asset_rec->id, owner ) );
      if( quote_asset_rec->is_restricted() )
         FC_ASSERT( eval_state._current_state->get_authorization( quote_asset_rec->id, owner ) );

      const bool issuer_override = quote_asset_rec->is_retractable() && eval_state.verify_authority( quote_asset_rec->authority );
      if( !issuer_override && !eval_state.check_signature( owner ) )
         FC_CAPTURE_AND_THROW( missing_signature, (bid_index.owner) );

      FC_ASSERT( !issuer_override && !quote_asset_rec->is_balance_frozen() );

      asset delta_amount  = this->get_amount();

      eval_state.validate_asset( delta_amount );

      auto current_bid   = eval_state._current_state->get_bid_record( this->bid_index );

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
          eval_state.sub_balance( balance_id_type(), delta_amount );
      }

      current_bid->last_update = eval_state._current_state->now();
      current_bid->balance     += this->amount;

      eval_state._current_state->store_bid_record( this->bid_index, *current_bid );

      //auto check   = eval_state._current_state->get_bid_record( this->bid_index );
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

   /**
    *  If the amount is negative then it will withdraw/cancel the bid assuming
    *  it is signed by the owner and there is sufficient funds.
    *
    *  If the amount is positive then it will add funds to the bid.
    */
   void relative_bid_operation::evaluate( transaction_evaluation_state& eval_state )const
   { try {
      if( this->bid_index.order_price == price() )
         FC_CAPTURE_AND_THROW( zero_price, (bid_index.order_price) );

      auto owner = this->bid_index.owner;
      if( !eval_state.check_signature( owner ) )
         FC_CAPTURE_AND_THROW( missing_signature, (bid_index.owner) );

      asset delta_amount  = this->get_amount();

      eval_state.validate_asset( delta_amount );
      auto quote_asset_rec = eval_state._current_state->get_asset_record( bid_index.order_price.quote_asset_id );
      FC_ASSERT( quote_asset_rec->is_market_issued() );
      FC_ASSERT( bid_index.order_price.base_asset_id == 0 ); // NOTE: only allowing issuance against base asset

      auto current_bid   = eval_state._current_state->get_relative_bid_record( this->bid_index );

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
          eval_state.sub_balance( balance_id_type(), delta_amount );
      }

      current_bid->last_update = eval_state._current_state->now();
      current_bid->balance     += this->amount;
      current_bid->limit_price =  this->limit_price;

      eval_state._current_state->store_relative_bid_record( this->bid_index, *current_bid );

      //auto check   = eval_state._current_state->get_bid_record( this->bid_index );
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

   /**
    *  If the amount is negative then it will withdraw/cancel the bid assuming
    *  it is signed by the owner and there is sufficient funds.
    *
    *  If the amount is positive then it will add funds to the bid.
    */
   void ask_operation::evaluate( transaction_evaluation_state& eval_state )const
   { try {
      if( this->ask_index.order_price == price() )
         FC_CAPTURE_AND_THROW( zero_price, (ask_index.order_price) );

      auto owner = this->ask_index.owner;

      auto base_asset_rec = eval_state._current_state->get_asset_record( ask_index.order_price.base_asset_id );
      auto quote_asset_rec = eval_state._current_state->get_asset_record( ask_index.order_price.quote_asset_id );
      FC_ASSERT( base_asset_rec.valid() );
      FC_ASSERT( quote_asset_rec.valid() );
      if( base_asset_rec->is_restricted() )
         FC_ASSERT( eval_state._current_state->get_authorization( base_asset_rec->id, owner ) );
      if( quote_asset_rec->is_restricted() )
         FC_ASSERT( eval_state._current_state->get_authorization( quote_asset_rec->id, owner ) );

      const bool issuer_override = base_asset_rec->is_retractable() && eval_state.verify_authority( base_asset_rec->authority );
      if( !issuer_override && !eval_state.check_signature( owner ) )
         FC_CAPTURE_AND_THROW( missing_signature, (ask_index.owner) );

      FC_ASSERT( !issuer_override && !base_asset_rec->is_balance_frozen() );

      asset delta_amount  = this->get_amount();

      eval_state.validate_asset( delta_amount );

      auto current_ask   = eval_state._current_state->get_ask_record( this->ask_index );

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
          eval_state.sub_balance( balance_id_type(), delta_amount );
      }

      current_ask->last_update = eval_state._current_state->now();
      current_ask->balance     += this->amount;
      FC_ASSERT( current_ask->balance >= 0, "", ("current_ask",current_ask)  );

      eval_state._current_state->store_ask_record( this->ask_index, *current_ask );
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

   /**
    *  If the amount is negative then it will withdraw/cancel the bid assuming
    *  it is signed by the owner and there is sufficient funds.
    *
    *  If the amount is positive then it will add funds to the bid.
    */
   void relative_ask_operation::evaluate( transaction_evaluation_state& eval_state )const
   { try {
      if( this->ask_index.order_price == price() )
         FC_CAPTURE_AND_THROW( zero_price, (ask_index.order_price) );

      FC_ASSERT( ask_index.order_price.quote_asset_id > ask_index.order_price.base_asset_id );

      auto owner = this->ask_index.owner;
      if( !eval_state.check_signature( owner ) )
         FC_CAPTURE_AND_THROW( missing_signature, (ask_index.owner) );

      asset delta_amount  = this->get_amount();

      eval_state.validate_asset( delta_amount );

      auto quote_asset_rec = eval_state._current_state->get_asset_record( ask_index.order_price.quote_asset_id );
      FC_ASSERT( quote_asset_rec->is_market_issued() );
      FC_ASSERT( ask_index.order_price.base_asset_id == 0 ); // NOTE: only allowing issuance against base asset

      auto current_ask   = eval_state._current_state->get_ask_record( this->ask_index );

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
          eval_state.sub_balance( balance_id_type(), delta_amount );
      }

      current_ask->last_update = eval_state._current_state->now();
      current_ask->balance     += this->amount;
      current_ask->limit_price =  this->limit_price;
      FC_ASSERT( current_ask->balance >= 0, "", ("current_ask",current_ask)  );

      eval_state._current_state->store_relative_ask_record( this->ask_index, *current_ask );
   } FC_CAPTURE_AND_RETHROW( (*this) ) }

   void short_operation::evaluate( transaction_evaluation_state& eval_state )const
   {
      auto owner = this->short_index.owner;
      FC_ASSERT( short_index.order_price.ratio < fc::uint128( 10, 0 ), "Interest rate must be less than 1000% APR" );
      FC_ASSERT( short_index.order_price.quote_asset_id > short_index.order_price.base_asset_id,
                 "Interest rate price must have valid base and quote IDs" );

      asset delta_amount  = this->get_amount();

      // Only allow using the base asset as collateral
      FC_ASSERT( delta_amount.asset_id == 0 );

      eval_state.validate_asset( delta_amount );
      auto  asset_to_short = eval_state._current_state->get_asset_record( short_index.order_price.quote_asset_id );
      FC_ASSERT( asset_to_short.valid() );
      FC_ASSERT( asset_to_short->is_market_issued(), "${symbol} is not a market issued asset", ("symbol",asset_to_short->symbol) );

      auto current_short   = eval_state._current_state->get_short_record( this->short_index );
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
          eval_state.sub_balance( balance_id_type(), delta_amount );
      }
      current_short->limit_price = this->short_index.limit_price;
      current_short->last_update = eval_state._current_state->now();
      current_short->balance     += this->amount;
      FC_ASSERT( current_short->balance >= 0 );

      eval_state._current_state->store_short_record( this->short_index, *current_short );
   }

   /**
     pay off part of the USD balance, if balance goes to 0 then close out
     the position and transfer collateral to proper place.
     update the call price (remove old value, add new value)
   */
   void cover_operation::evaluate( transaction_evaluation_state& eval_state )const
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
      eval_state.sub_balance( balance_id_type(), delta_amount );

      auto current_cover   = eval_state._current_state->get_collateral_record( this->cover_index );
      if( NOT current_cover )
         FC_CAPTURE_AND_THROW( unknown_market_order, (cover_index) );

      auto  asset_to_cover = eval_state._current_state->get_asset_record( cover_index.order_price.quote_asset_id );
      FC_ASSERT( asset_to_cover.valid() );

      const auto start_time = current_cover->expiration - fc::seconds( BTS_BLOCKCHAIN_MAX_SHORT_PERIOD_SEC );
      auto elapsed_sec = ( eval_state._current_state->now() - start_time ).to_seconds();
      if( elapsed_sec < 0 ) elapsed_sec = 0;

      const asset principle = asset( current_cover->payoff_balance, delta_amount.asset_id );
      asset total_debt = detail::market_engine::get_interest_owed( principle, current_cover->interest_rate, elapsed_sec ) + principle;

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
          principle_paid = delta_amount - interest_paid;
          current_cover->payoff_balance -= principle_paid.amount;
      }
      FC_ASSERT( principle_paid.amount >= 0 );
      FC_ASSERT( interest_paid.amount >= 0 );
      FC_ASSERT( current_cover->payoff_balance >= 0 );

      //Covered asset is destroyed, interest pays to fees
      asset_to_cover->current_share_supply -= principle_paid.amount;
      asset_to_cover->collected_fees += interest_paid.amount;
      eval_state._current_state->store_asset_record( *asset_to_cover );

      // changing the payoff balance changes the call price... so we need to remove the old record
      // and insert a new one.
      eval_state._current_state->store_collateral_record( this->cover_index, collateral_record() );

      FC_ASSERT( current_cover->interest_rate.quote_asset_id > current_cover->interest_rate.base_asset_id,
                 "Rejecting cover order with invalid interest rate.", ("cover", *current_cover) );

      if( current_cover->payoff_balance > 0 )
      {
         const auto new_call_price = asset( current_cover->payoff_balance, delta_amount.asset_id)
                                     / asset( (current_cover->collateral_balance * BTS_BLOCKCHAIN_MCALL_D2C_NUMERATOR)
                                            / BTS_BLOCKCHAIN_MCALL_D2C_DENOMINATOR,
                                            cover_index.order_price.base_asset_id );

         if( this->new_cover_price && (*this->new_cover_price > new_call_price) )
            eval_state._current_state->store_collateral_record( market_index_key( *this->new_cover_price, this->cover_index.owner ),
                                                                *current_cover );
         else
            eval_state._current_state->store_collateral_record( market_index_key( new_call_price, this->cover_index.owner ),
                                                                *current_cover );
      }
      else // withdraw the collateral to the transaction to be deposited at owners discretion / cover fees
      {
         if( current_cover->slate_id && cover_index.order_price.base_asset_id == 0 )
         {
            eval_state.adjust_vote( current_cover->slate_id, -current_cover->collateral_balance );
         }
         eval_state.add_balance( asset( current_cover->collateral_balance, cover_index.order_price.base_asset_id ) );
      }
   }

   void add_collateral_operation::evaluate( transaction_evaluation_state& eval_state )const
   {
      if( this->cover_index.order_price == price() )
         FC_CAPTURE_AND_THROW( zero_price, (cover_index.order_price) );

      if( this->amount == 0 )
         FC_CAPTURE_AND_THROW( zero_amount );

      if( this->amount < 0 )
         FC_CAPTURE_AND_THROW( negative_deposit );

      asset delta_amount  = this->get_amount();
      eval_state.sub_balance( balance_id_type(), delta_amount );

      // update collateral and call price
      auto current_cover = eval_state._current_state->get_collateral_record( this->cover_index );
      if( NOT current_cover )
         FC_CAPTURE_AND_THROW( unknown_market_order, (cover_index) );

      current_cover->collateral_balance += delta_amount.amount;

      const auto min_call_price = asset( current_cover->payoff_balance, cover_index.order_price.quote_asset_id )
                                  / asset( (current_cover->collateral_balance * BTS_BLOCKCHAIN_MCALL_D2C_NUMERATOR)
                                  / BTS_BLOCKCHAIN_MCALL_D2C_DENOMINATOR,
                                  cover_index.order_price.base_asset_id );
      const auto new_call_price = std::min( min_call_price, cover_index.order_price );

      // changing the payoff balance changes the call price... so we need to remove the old record
      // and insert a new one.
      eval_state._current_state->store_collateral_record( this->cover_index, collateral_record() );


      eval_state._current_state->store_collateral_record( market_index_key( new_call_price, this->cover_index.owner),
                                                          *current_cover );
   }

   void update_cover_operation::evaluate( transaction_evaluation_state& eval_state )const
   {
      if( this->cover_index.order_price == price() )
         FC_CAPTURE_AND_THROW( zero_price, (cover_index.order_price) );

      if( this->new_call_price == price() )
         FC_CAPTURE_AND_THROW( zero_price, (new_call_price) );

      FC_ASSERT( this->new_call_price.quote_asset_id == cover_index.order_price.quote_asset_id );
      FC_ASSERT( this->new_call_price.base_asset_id == cover_index.order_price.base_asset_id );

      // update collateral and call price
      auto current_cover = eval_state._current_state->get_collateral_record( this->cover_index );
      if( NOT current_cover )
         FC_CAPTURE_AND_THROW( unknown_market_order, (cover_index) );

      const auto min_call_price = asset( current_cover->payoff_balance, cover_index.order_price.quote_asset_id )
                                  / asset( (current_cover->collateral_balance * BTS_BLOCKCHAIN_MCALL_D2C_NUMERATOR)
                                  / BTS_BLOCKCHAIN_MCALL_D2C_DENOMINATOR, cover_index.order_price.base_asset_id );

      FC_ASSERT( new_call_price >= min_call_price );


      // changing the payoff balance changes the call price... so we need to remove the old record
      // and insert a new one.
      eval_state._current_state->store_collateral_record( this->cover_index, collateral_record() );
      if( this->new_slate && *this->new_slate != current_cover->slate_id )
      {
         eval_state.adjust_vote( current_cover->slate_id, -current_cover->collateral_balance );
         current_cover->slate_id = *this->new_slate;
         eval_state.adjust_vote( current_cover->slate_id, current_cover->collateral_balance );
      }
      eval_state._current_state->store_collateral_record( market_index_key( new_call_price, this->new_owner ? *this->new_owner : this->cover_index.owner),
                                                          *current_cover );
   }

} } // bts::blockchain
