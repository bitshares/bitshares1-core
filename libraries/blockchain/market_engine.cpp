/** This file is designed to be included in detail::chain_database_impl, but is pulled into a separate file for
 * ease of maintenance and upgrade.
 */

class market_engine
{
   public:
      market_engine( pending_chain_state_ptr ps, chain_database_impl& cdi )
      :_pending_state(ps),_db_impl(cdi),_orders_filled(0)
      {
         _pending_state = std::make_shared<pending_chain_state>( ps );
         _prior_state = ps;
      }

      void cancel_all_shorts()
      {
         for( auto short_itr = _db_impl._short_db.begin(); short_itr.valid(); ++short_itr )
         {
             const auto market_idx = short_itr.key();
             const auto quote_asset = _pending_state->get_asset_record( market_idx.order_price.quote_asset_id );
             _current_bid = market_order( short_order, short_itr.key(), short_itr.value() );

             market_transaction mtrx;
             cancel_current_short( mtrx, *quote_asset );
             push_market_transaction( mtrx );
         }
      }

      void execute( asset_id_type quote_id, asset_id_type base_id, const fc::time_point_sec& timestamp )
      {
         try {
             const auto pending_block_num = _pending_state->get_head_block_num();

             _quote_id = quote_id;
             _base_id = base_id;
             auto quote_asset = _pending_state->get_asset_record( _quote_id );
             auto base_asset = _pending_state->get_asset_record( _base_id );
             FC_ASSERT( quote_asset.valid() && base_asset.valid() );

             // the order book is sorted from low to high price, so to get the last item (highest bid), we need to go to the first item in the
             // next market class and then back up one
             auto next_pair  = base_id+1 == quote_id ? price( 0, quote_id+1, 0) : price( 0, quote_id, base_id+1 );
             _bid_itr        = _db_impl._bid_db.lower_bound( market_index_key( next_pair ) );
             _ask_itr        = _db_impl._ask_db.lower_bound( market_index_key( price( 0, quote_id, base_id) ) );
             _short_itr      = _db_impl._short_db.lower_bound( market_index_key( price( 0, quote_id, base_id) ) );
             _collateral_itr = _db_impl._collateral_db.lower_bound( market_index_key( next_pair ) );

             if( !_ask_itr.valid() )
             {
                wlog( "ask iter invalid..." );
                _ask_itr = _db_impl._ask_db.begin();
             }

             if( _bid_itr.valid() )   --_bid_itr;
             else _bid_itr = _db_impl._bid_db.last();

             if( _collateral_itr.valid() )   --_collateral_itr;
             else _collateral_itr = _db_impl._collateral_db.last();

             asset trading_volume(0, base_id);

             omarket_status market_stat = _pending_state->get_market_status( _quote_id, _base_id );
             if( !market_stat ) market_stat = market_status( quote_id, base_id, 0, 0 );
             _market_stat = *market_stat;

             price max_short_bid;
             price min_cover_ask;
             price opening_price;
             price closing_price;

             // while bootstraping we use this metric
             auto median_price = _db_impl.self->get_median_delegate_price( quote_id );

             // convert any fees collected in quote unit to XTS
             if( base_id == 0 )
             {
                if( quote_asset->is_market_issued() )
                {
                   if( market_stat->avg_price_1h.ratio == fc::uint128_t() )
                   {
                      if( !median_price )
                      {
                         FC_CAPTURE_AND_THROW( insufficient_feeds, (quote_id) );
                      }
                      else
                      {
                         market_stat->avg_price_1h = *median_price;
                         _market_stat = *market_stat;
                      }
                   }
                   min_cover_ask = market_stat->minimum_ask();

                   if( median_price )
                      max_short_bid = *median_price;
                   else
                      max_short_bid = market_stat->avg_price_1h;
                }
             }


             int last_orders_filled = -1;

             bool order_did_execute = false;
             // prime the pump
             get_next_bid(); get_next_ask();
             get_next_bid(); get_next_ask();
             idump( (_current_bid)(_current_ask) );
             while( get_next_bid() && get_next_ask() )
             {
                idump( (_current_bid)(_current_ask) );

                // make sure that at least one order was matched
                // every time we enter the loop.
                if( _orders_filled == last_orders_filled )
                {
                   FC_ASSERT( _orders_filled != last_orders_filled, "we appear caught in a order matching loop" );
                }
                last_orders_filled = _orders_filled;

                auto bid_quantity_xts = _current_bid->get_quantity();
                auto ask_quantity_xts = _current_ask->get_quantity();

                asset current_bid_balance  = _current_bid->get_balance();

                /** the market transaction we are filling out */
                market_transaction mtrx;
                mtrx.bid_price = _current_bid->get_price();
                mtrx.ask_price = _current_ask->get_price();
                mtrx.bid_owner = _current_bid->get_owner();
                mtrx.ask_owner = _current_ask->get_owner();
                mtrx.bid_type  = _current_bid->type;
                mtrx.ask_type  = _current_ask->type;

                if( _current_ask->type == cover_order && _current_bid->type == short_order )
                {
                   mtrx.bid_price = _market_stat.avg_price_1h;
                   mtrx.ask_price = _market_stat.avg_price_1h;
                   FC_ASSERT( quote_asset->is_market_issued() && base_id == 0 );
                   if( mtrx.ask_price < mtrx.bid_price ) // the call price has not been reached
                      break;

                   if( _current_bid->state.short_price_limit.valid() )
                   {
                      if( *_current_bid->state.short_price_limit < mtrx.bid_price )
                      {
                         elog( "." );
                         _current_bid.reset();
                         continue; // skip shorts that are over the price limit.
                      }
                   }

                   auto collateral_rate = _current_bid->get_price();
                   if( collateral_rate > market_stat->avg_price_1h )
                      collateral_rate = market_stat->avg_price_1h;

                   auto ask_quantity_usd   = _current_ask->get_quote_quantity();
                   auto short_quantity_usd = _current_bid->get_balance() / collateral_rate;

                   auto trade_quantity_usd = std::min(short_quantity_usd,ask_quantity_usd);

                   mtrx.ask_received   = trade_quantity_usd;
                   mtrx.bid_paid       = trade_quantity_usd;
                   mtrx.ask_paid       = mtrx.ask_received * mtrx.ask_price;
                   mtrx.bid_received   = mtrx.ask_paid;
                   mtrx.bid_collateral = mtrx.bid_paid / collateral_rate;

                   // check for rounding errors
                   if( (*mtrx.bid_collateral - _current_bid->get_balance()).amount < BTS_BLOCKCHAIN_PRECISION )
                       mtrx.bid_collateral = _current_bid->get_balance();

                   if( *mtrx.bid_collateral < mtrx.ask_paid )
                   {
                       elog( "Skipping Short for Insufficient Collateral" );
                       edump( (mtrx) );
                       _current_bid.reset(); // skip this bid, its price is too low
                       continue;
                       market_stat->bid_depth -= mtrx.bid_collateral->amount;
                       // cancel short... too little collateral at these prices
                       cancel_current_short( mtrx, *quote_asset );
                   }

                   order_did_execute = true;
                   pay_current_short( mtrx, *quote_asset );
                   pay_current_cover( mtrx, *quote_asset );

                   market_stat->bid_depth -= mtrx.bid_collateral->amount;
                   market_stat->ask_depth += mtrx.bid_collateral->amount;
                }
                else if( _current_ask->type == cover_order && _current_bid->type == bid_order )
                {
                   //elog( "CURRENT ASK IS COVER" );
                   FC_ASSERT( quote_asset->is_market_issued() && base_id == 0 );
                   if( mtrx.ask_price < mtrx.bid_price )
                      break; // the call price has not been reached

                   /**
                    *  Don't allow margin calls to be executed too far below
                    *  the minimum ask, this could lead to an attack where someone
                    *  walks the whole book to steal the collateral.
                    */
                   if( mtrx.bid_price < market_stat->minimum_ask() )
                   {
                      _current_ask.reset();
                      continue;
                   }

                   mtrx.ask_price = mtrx.bid_price;

                   auto max_usd_purchase = asset(*_current_ask->collateral,0) * mtrx.bid_price;
                   auto usd_exchanged = std::min( current_bid_balance, max_usd_purchase );

                   const auto required_usd_purchase = _current_ask->get_balance();
                   if( required_usd_purchase < usd_exchanged )
                      usd_exchanged = required_usd_purchase;

                   mtrx.bid_paid     = usd_exchanged;
                   mtrx.ask_received = usd_exchanged;

                   // handle rounding errors
                   if( usd_exchanged == max_usd_purchase )
                      mtrx.ask_paid     = asset(*_current_ask->collateral,0);
                   else
                      mtrx.ask_paid = usd_exchanged * mtrx.bid_price;

                   mtrx.bid_received = mtrx.ask_paid;


                   order_did_execute = true;
                   pay_current_bid( mtrx, *quote_asset );
                   pay_current_cover( mtrx, *quote_asset );

                   market_stat->ask_depth -= mtrx.ask_paid.amount;
                }
                else if( _current_ask->type == ask_order && _current_bid->type == short_order )
                {
                   mtrx.bid_price = _market_stat.avg_price_1h;
                   FC_ASSERT( quote_asset->is_market_issued() && base_id == 0 );
                   if( mtrx.bid_price < mtrx.ask_price ) // the ask asn't been reached.
                      break;

                   if( _current_bid->state.short_price_limit.valid() )
                   {
                      if( *_current_bid->state.short_price_limit < mtrx.bid_price )
                      {
                         elog( "short price limit < bid price" );
                         _current_bid.reset();
                         continue; // skip shorts that are over the price limit.
                      }
                   }

                   auto collateral_rate = _current_bid->get_price();
                   if( collateral_rate > market_stat->avg_price_1h )
                      collateral_rate = market_stat->avg_price_1h;

                   auto ask_quantity_usd   = _current_ask->get_quote_quantity();
                   auto short_quantity_usd = _current_bid->get_balance() / collateral_rate;

                   auto trade_quantity_usd = std::min(short_quantity_usd,ask_quantity_usd);

                   mtrx.ask_received   = trade_quantity_usd;
                   mtrx.ask_paid       = mtrx.ask_received * mtrx.ask_price;
                   mtrx.bid_paid       = trade_quantity_usd;
                   mtrx.bid_received   = mtrx.ask_paid;
                   mtrx.bid_collateral = mtrx.bid_paid / collateral_rate;

                   // check for rounding errors
                   //if( (mtrx.ask_paid - _current_ask->get_balance()).amount < BTS_BLOCKCHAIN_PRECISION )
                   //   mtrx.ask_paid = _current_ask->get_balance();

                   if( (*mtrx.bid_collateral - _current_bid->get_balance()).amount < BTS_BLOCKCHAIN_PRECISION )
                       mtrx.bid_collateral = _current_bid->get_balance();


                   if( *mtrx.bid_collateral < mtrx.ask_paid )
                   {
                       elog( "Skipping Short for Insufficient Collateral" );
                       edump( (mtrx) );
                       _current_bid.reset(); // skip this bid, its price is too low
                       continue;
                       market_stat->bid_depth -= mtrx.bid_collateral->amount;
                       // cancel short... too little collateral at these prices
                       cancel_current_short( mtrx, *quote_asset );
                       market_stat->bid_depth -= mtrx.bid_received.amount;
                   }

                   order_did_execute = true;
                   pay_current_short( mtrx, *quote_asset );
                   pay_current_ask( mtrx, *quote_asset );
                   market_stat->bid_depth -= mtrx.bid_collateral->amount;
                   market_stat->ask_depth += mtrx.bid_collateral->amount;

                }
                else if( _current_ask->type == ask_order && _current_bid->type == bid_order )
                {
                   if( mtrx.bid_price < mtrx.ask_price ) break;
                   auto quantity_xts = std::min( bid_quantity_xts, ask_quantity_xts );

                   mtrx.bid_paid       = quantity_xts * mtrx.bid_price;
                   // ask gets exactly what they asked for
                   mtrx.ask_received   = quantity_xts * mtrx.ask_price;
                   mtrx.ask_paid       = quantity_xts;
                   mtrx.bid_received   = quantity_xts;

                   // because there could be rounding errors, we assume that if we are
                   // filling the bid quantity we are paying the full balance rather
                   // than suffer rounding errors.
                   if( quantity_xts == bid_quantity_xts )
                   {
                      mtrx.bid_paid = current_bid_balance;
                   }

                   order_did_execute = true;
                   pay_current_bid( mtrx, *quote_asset );
                   pay_current_ask( mtrx, *base_asset );

                   market_stat->ask_depth -= mtrx.ask_paid.amount;
                   mtrx.fees_collected = mtrx.bid_paid - mtrx.ask_received;
                }

                push_market_transaction(mtrx);
                if( mtrx.ask_received.asset_id == 0 )
                  trading_volume += mtrx.ask_received;
                else if( mtrx.bid_received.asset_id == 0 )
                  trading_volume += mtrx.bid_received;
                if( opening_price == price() )
                  opening_price = mtrx.bid_price;
                closing_price = mtrx.bid_price;

                if( mtrx.fees_collected.asset_id == base_asset->id )
                    base_asset->collected_fees += mtrx.fees_collected.amount;
                else if( mtrx.fees_collected.asset_id == quote_asset->id )
                    quote_asset->collected_fees += mtrx.fees_collected.amount;
             } // while( next bid && next ask )


             // update any fees collected
             _pending_state->store_asset_record( *quote_asset );
             _pending_state->store_asset_record( *base_asset );

             market_stat->last_error.reset();

             order_did_execute |= (pending_block_num % 6) == 0;

             if( _current_bid && _current_ask && order_did_execute )
             {
                if( median_price )
                {
                   market_stat->avg_price_1h = *median_price;
                }
                else if( _current_bid->type != short_order ) // we cannot use short prices for this
                {
                   // after the market is running solid we can use this metric...
                   market_stat->avg_price_1h.ratio *= (BTS_BLOCKCHAIN_BLOCKS_PER_HOUR-1);

                   const auto max_bid = market_stat->maximum_bid();

                   // limit the maximum movement rate of the price.
                   if( _current_bid->get_price() < min_cover_ask )
                      market_stat->avg_price_1h.ratio += min_cover_ask.ratio;
                   else if( _current_bid->get_price() > max_bid )
                      market_stat->avg_price_1h.ratio += max_bid.ratio; //max_short_bid.ratio;
                   else
                      market_stat->avg_price_1h.ratio += _current_bid->get_price().ratio;

                   if( _current_ask->get_price() < min_cover_ask )
                      market_stat->avg_price_1h.ratio += min_cover_ask.ratio;
                   else if( _current_ask->get_price() > max_bid )
                      market_stat->avg_price_1h.ratio += max_bid.ratio;
                   else
                      market_stat->avg_price_1h.ratio += _current_ask->get_price().ratio;

                   market_stat->avg_price_1h.ratio /= (BTS_BLOCKCHAIN_BLOCKS_PER_HOUR+1);
                }
             }

             if( quote_asset->is_market_issued() && base_id == 0 )
             {
                if( market_stat->ask_depth < BTS_BLOCKCHAIN_MARKET_DEPTH_REQUIREMENT
                    || market_stat->bid_depth < BTS_BLOCKCHAIN_MARKET_DEPTH_REQUIREMENT )
                {
                   _market_transactions.clear(); // nothing should have executed
                  std::string reason = "After executing orders there was insufficient depth remaining";
                  FC_CAPTURE_AND_THROW( insufficient_depth, (reason)(market_stat)(BTS_BLOCKCHAIN_MARKET_DEPTH_REQUIREMENT) );
                }
             }
             _pending_state->store_market_status( *market_stat );

             update_market_history( trading_volume, opening_price, closing_price, market_stat, timestamp );

             wlog( "done matching orders" );
             idump( (_current_bid)(_current_ask) );
             _pending_state->apply_changes();
        }
        catch( const fc::exception& e )
        {
           wlog( "error executing market ${quote} / ${base}\n ${e}", ("quote",quote_id)("base",base_id)("e",e.to_detail_string()) );
           auto market_state = _prior_state->get_market_status( quote_id, base_id );
           if( !market_state )
              market_state = market_status( quote_id, base_id, 0, 0 );
           market_state->last_error = e;
           _prior_state->store_market_status( *market_state );
        }
      } // execute(...)
      void push_market_transaction( const market_transaction& mtrx )
      { try {
          if( mtrx.ask_paid.amount != 0
              || mtrx.ask_received.amount != 0
              || mtrx.bid_received.asset_id != 0
              || mtrx.bid_paid.amount != 0 )
          {
              // These checks only apply for non-cancel orders
              FC_ASSERT( mtrx.bid_paid.amount >= 0 );
              FC_ASSERT( mtrx.ask_paid.amount >= 0 );
              FC_ASSERT( mtrx.bid_received.amount >= 0 );
              FC_ASSERT( mtrx.ask_received .amount>= 0 );
              FC_ASSERT( mtrx.bid_paid >= mtrx.ask_received );
              FC_ASSERT( mtrx.ask_paid >= mtrx.bid_received );
              FC_ASSERT( mtrx.fees_collected.amount >= 0 );
          }

          wlog( "${trx}", ("trx", fc::json::to_pretty_string( mtrx ) ) );

          _market_transactions.push_back(mtrx);
      } FC_CAPTURE_AND_RETHROW( (mtrx) ) }

      void cancel_current_short( market_transaction& mtrx, const asset_record& quote_asset  )
      {
         mtrx.ask_paid       = asset();
         mtrx.ask_received   = asset(0,quote_asset.id);
         mtrx.bid_received   = _current_bid->get_balance();
         mtrx.bid_paid       = asset(0,quote_asset.id);
         mtrx.bid_collateral.reset();

         auto bid_payout = _pending_state->get_balance_record(
                                   withdraw_condition( withdraw_with_signature(mtrx.bid_owner), _base_id ).get_address() );

         if( !bid_payout )
            bid_payout = balance_record( mtrx.bid_owner, asset(0,_base_id), 0 );

         bid_payout->balance += mtrx.bid_received.amount;
         bid_payout->last_update = _pending_state->now();
         bid_payout->deposit_date = _pending_state->now();
         _pending_state->store_balance_record( *bid_payout );

         _current_bid->state.balance = 0;
      }

      void pay_current_short(const market_transaction& mtrx, asset_record& quote_asset  )
      { try {
          FC_ASSERT( _current_bid->type == short_order );
          FC_ASSERT( mtrx.bid_type == short_order );

          /** NOTE: the short may pay extra XTS to the collateral in the event rounding occurs.  This
           * just checks to make sure it is reasonable.
           */
          FC_ASSERT( mtrx.ask_paid <= *mtrx.bid_collateral, "", ("mtrx",mtrx) );

          quote_asset.current_share_supply += mtrx.bid_paid.amount;

          auto collateral  = *mtrx.bid_collateral + mtrx.ask_paid;
          if( mtrx.bid_paid.amount <= 0 ) // WHY is this ever negitive??
          {
             FC_ASSERT( mtrx.bid_paid.amount >= 0 );
             _current_bid->state.balance -= mtrx.bid_collateral->amount;
             return;
          }

          auto call_collateral = collateral;
          call_collateral.amount *= 2;
          call_collateral.amount /= 3;
          //auto cover_price = mtrx.bid_price;
          auto cover_price = mtrx.bid_paid / call_collateral;
          //cover_price.ratio *= 2;
          //cover_price.ratio /= 3;
         // auto cover_price = mtrx.bid_paid / asset( (3*collateral.amount)/4, _base_id );

          market_index_key cover_index( cover_price, _current_bid->get_owner() );
          auto ocover_record = _pending_state->get_collateral_record( cover_index );

          if( NOT ocover_record ) ocover_record = collateral_record();

          ocover_record->collateral_balance += collateral.amount;
          ocover_record->payoff_balance += mtrx.bid_paid.amount;

          FC_ASSERT( ocover_record->payoff_balance >= 0, "", ("record",ocover_record) );
          FC_ASSERT( ocover_record->collateral_balance >= 0 , "", ("record",ocover_record));

          _current_bid->state.balance -= mtrx.bid_collateral->amount;
          FC_ASSERT( _current_bid->state.balance >= 0 );

          _pending_state->store_collateral_record( cover_index, *ocover_record );

          _pending_state->store_short_record( _current_bid->market_index, _current_bid->state );
      } FC_CAPTURE_AND_RETHROW( (mtrx)  ) }

      void pay_current_bid( const market_transaction& mtrx, asset_record& quote_asset )
      { try {
          FC_ASSERT( _current_bid->type == bid_order );
          FC_ASSERT( mtrx.bid_type == bid_order );
          _current_bid->state.balance -= mtrx.bid_paid.amount;
          FC_ASSERT( _current_bid->state.balance >= 0 );

          auto bid_payout = _pending_state->get_balance_record(
                                    withdraw_condition( withdraw_with_signature(mtrx.bid_owner), _base_id ).get_address() );
          if( !bid_payout )
             bid_payout = balance_record( mtrx.bid_owner, asset(0,_base_id), 0 );

          bid_payout->balance += mtrx.bid_received.amount;
          bid_payout->last_update = _pending_state->now();
          bid_payout->deposit_date = _pending_state->now();
          _pending_state->store_balance_record( *bid_payout );


          // if the balance is less than 1 XTS then it gets collected as fees.
          if( (_current_bid->get_quote_quantity() * _current_bid->get_price()).amount == 0 )
          {
              quote_asset.collected_fees += _current_bid->get_quote_quantity().amount;
              _current_bid->state.balance = 0;
          }
          _pending_state->store_bid_record( _current_bid->market_index, _current_bid->state );
      } FC_CAPTURE_AND_RETHROW( (mtrx) ) }

      void pay_current_cover( market_transaction& mtrx, asset_record& quote_asset )
      { try {
          FC_ASSERT( _current_ask->type == cover_order );
          FC_ASSERT( mtrx.ask_type == cover_order );

          // we are in the margin call range...
          _current_ask->state.balance  -= mtrx.bid_paid.amount;
          *(_current_ask->collateral)  -= mtrx.ask_paid.amount;

          FC_ASSERT( _current_ask->state.balance >= 0 );
          FC_ASSERT( *_current_ask->collateral >= 0, "", ("mtrx",mtrx)("_current_ask", _current_ask)  );

          quote_asset.current_share_supply -= mtrx.ask_received.amount;
          if( *_current_ask->collateral == 0 )
          {
              quote_asset.collected_fees -= _current_ask->state.balance;
              _current_ask->state.balance = 0;
          }


          if( _current_ask->state.balance == 0 && *_current_ask->collateral > 0 ) // no more USD left
          { // send collateral home to mommy & daddy
                //wlog( "            collateral balance is now 0!" );
                auto ask_balance_address = withdraw_condition(
                                                  withdraw_with_signature(_current_ask->get_owner()),
                                                  _base_id ).get_address();

                auto ask_payout = _pending_state->get_balance_record( ask_balance_address );
                if( !ask_payout )
                   ask_payout = balance_record( _current_ask->get_owner(), asset(0,_base_id), 0 );

                auto left_over_collateral = (*_current_ask->collateral);

                /** charge 5% fee for having a margin call */
                auto fee = (left_over_collateral * 5000 )/100000;
                left_over_collateral -= fee;
                // when executing a cover order, it always takes the exact price of the
                // highest bid, so there should be no fees paid *except* this.
                FC_ASSERT( mtrx.fees_collected.amount == 0 );

                // these go to the network... as dividends..
                mtrx.fees_collected  += asset(fee,0);

                ask_payout->balance += left_over_collateral;
                ask_payout->last_update = _pending_state->now();
                ask_payout->deposit_date = _pending_state->now();

                _pending_state->store_balance_record( *ask_payout );
                _current_ask->collateral = 0;
          }
          //ulog( "storing collateral ${c}", ("c",_current_ask) );

          // the collateral position is now worse than before, if we don't update the market index then
          // the index price will be "wrong"... ie: the call price should move up based upon the fact
          // that we consumed more collateral than USD...
          //
          // If we leave it as is, then chances are we will end up covering the entire amount this time,
          // but we cannot use the price on the call for anything other than a trigger.
          _pending_state->store_collateral_record( _current_ask->market_index,
                                                   collateral_record( *_current_ask->collateral,
                                                                      _current_ask->state.balance ) );
      } FC_CAPTURE_AND_RETHROW( (mtrx) ) }

      void pay_current_ask( const market_transaction& mtrx, asset_record& base_asset )
      { try {
          FC_ASSERT( _current_ask->type == ask_order ); // update ask + payout

          _current_ask->state.balance -= mtrx.ask_paid.amount;
          FC_ASSERT( _current_ask->state.balance >= 0 );

          auto ask_balance_address = withdraw_condition( withdraw_with_signature(mtrx.ask_owner), _quote_id ).get_address();
          auto ask_payout = _pending_state->get_balance_record( ask_balance_address );
          if( !ask_payout )
             ask_payout = balance_record( mtrx.ask_owner, asset(0,_quote_id), 0 );
          ask_payout->balance += mtrx.ask_received.amount;
          ask_payout->last_update = _pending_state->now();
          ask_payout->deposit_date = _pending_state->now();

          _pending_state->store_balance_record( *ask_payout );

          // if the balance is less than 1 XTS * PRICE < .001 USD XTS goes to fees
          if( (_current_ask->get_quantity() * _current_ask->get_price()).amount == 0 )
          {
              base_asset.collected_fees += _current_ask->get_quantity().amount;
              _current_ask->state.balance = 0;
          }
          _pending_state->store_ask_record( _current_ask->market_index, _current_ask->state );

      } FC_CAPTURE_AND_RETHROW( (mtrx) )  } // pay_current_ask

      bool get_next_bid()
      { try {
         if( _current_bid && _current_bid->get_quantity().amount > 0 )
            return _current_bid.valid();

         ++_orders_filled;
         _current_bid.reset();

         /** while there is an ask less than the avg price, then shorts take priority
          * in order of the collateral (XTS) per USD.  The "price" field is represented
          * as USD per XTS thus we want 1/price which means that lower prices are
          * higher collateral.  Therefore, we start low and move high through the
          * short order book.
          */
         if( _current_ask && _current_ask->get_price() <= _market_stat.avg_price_1h )
         {
            if( _short_itr.valid() )
            {
               auto bid = market_order( short_order, _short_itr.key(), _short_itr.value() );
               if( bid.get_price().quote_asset_id == _quote_id &&
                   bid.get_price().base_asset_id == _base_id )
               {
                   ++_short_itr;
                   _current_bid = bid;
                   return _current_bid.valid();
               }
            }
         }

         if( _bid_itr.valid() )
         {
            auto bid = market_order( bid_order, _bid_itr.key(), _bid_itr.value() );
            if( bid.get_price().quote_asset_id == _quote_id &&
                bid.get_price().base_asset_id == _base_id )
            {
                _current_bid = bid;
                --_bid_itr;
            }
         }
         return _current_bid.valid();
      } FC_CAPTURE_AND_RETHROW() }

      bool get_next_ask()
      { try {
         if( _current_ask && _current_ask->state.balance > 0 )
         {
            //idump( (_current_ask) );
            return _current_ask.valid();
         }
         _current_ask.reset();
         ++_orders_filled;

         /**
          *  Margin calls take priority over all other ask orders
          */
         while( _current_bid && _collateral_itr.valid() )
         {
            auto cover_ask = market_order( cover_order,
                                     _collateral_itr.key(),
                                     order_record(_collateral_itr.value().payoff_balance),
                                     _collateral_itr.value().collateral_balance  );

            if( cover_ask.get_price().quote_asset_id == _quote_id &&
                cover_ask.get_price().base_asset_id == _base_id )
            {
#if 0 // we will cover at any price that is within range of the price feed.
                if( _current_bid->get_price() < cover_ask.get_highest_cover_price()  )
                {
                   // cover position has been blown out, current bid is not able to
                   // cover the position, so it will sit until the price recovers
                   // enough to fill it.
                   //
                   // The idea here is that the longs have agreed to a maximum
                   // protection equal to the collateral.  If they would like to
                   // sell their USD for XTS this is the best price the short is
                   // obligated to offer.
                   //
                   // In other words, this ask MUST be filled before any thing else
                   FC_CAPTURE_AND_THROW( insufficient_collateral, (_current_bid)(cover_ask)(cover_ask.get_highest_cover_price()));
                   --_collateral_itr;
                   continue;
                }
#endif
                // max bid must be greater than call price
                if( _current_bid->get_price() < cover_ask.get_price() )
                {
                 //  if( _current_ask->get_price() > cover_ask.get_price() )
                   {
                      _current_ask = cover_ask;
                      _current_payoff_balance = _collateral_itr.value().payoff_balance;
                      //wlog( "--collateral_iter" );
                      --_collateral_itr;
                      //idump( (_current_ask) );
                      return _current_ask.valid();
                   }
                }
            }
            break;
         }

         if( _ask_itr.valid() )
         {
            auto ask = market_order( ask_order, _ask_itr.key(), _ask_itr.value() );
            //wlog( "ASK ITER VALID: ${o}", ("o",ask) );
            if( ask.get_price().quote_asset_id == _quote_id &&
                ask.get_price().base_asset_id == _base_id )
            {
                _current_ask = ask;
            }
            ++_ask_itr;
         }
         return _current_ask.valid();
      } FC_CAPTURE_AND_RETHROW() }


      /**
       *  This method should not affect market execution or validation and
       *  is for historical purposes only.
       */
      void update_market_history( const asset& trading_volume,
                                  const price& opening_price,
                                  const price& closing_price,
                                  const omarket_status& market_stat,
                                  const fc::time_point_sec& timestamp )
      {
             if( trading_volume.amount > 0 && get_next_bid() && get_next_ask() )
             {
               market_history_key key(_quote_id, _base_id, market_history_key::each_block, _db_impl._head_block_header.timestamp);
               market_history_record new_record(_current_bid->get_price(),
                                                _current_ask->get_price(),
                                                opening_price,
                                                closing_price,
                                                trading_volume.amount);

               FC_ASSERT( market_stat );
               new_record.recent_average_price = market_stat->avg_price_1h;

               //LevelDB iterators are dumb and don't support proper past-the-end semantics.
               auto last_key_itr = _db_impl._market_history_db.lower_bound(key);
               if( !last_key_itr.valid() )
                 last_key_itr = _db_impl._market_history_db.last();
               else
                 --last_key_itr;

               key.timestamp = timestamp;

               //Unless the previous record for this market is the same as ours...
               if( (!(last_key_itr.valid()
                   && last_key_itr.key().quote_id == _quote_id
                   && last_key_itr.key().base_id == _base_id
                   && last_key_itr.key().granularity == market_history_key::each_block
                   && last_key_itr.value() == new_record)) )
               {
                 //...add a new entry to the history table.
                 _pending_state->market_history[key] = new_record;
               }

               fc::time_point_sec start_of_this_hour = timestamp - (timestamp.sec_since_epoch() % (60*60));
               market_history_key old_key(_quote_id, _base_id, market_history_key::each_hour, start_of_this_hour);
               if( auto opt = _db_impl._market_history_db.fetch_optional(old_key) )
               {
                 auto old_record = *opt;
                 old_record.volume += new_record.volume;
                 if( new_record.highest_bid > old_record.highest_bid || new_record.lowest_ask < old_record.lowest_ask )
                 {
                   old_record.highest_bid = std::max(new_record.highest_bid, old_record.highest_bid);
                   old_record.lowest_ask = std::min(new_record.lowest_ask, old_record.lowest_ask);
                   old_record.recent_average_price = new_record.recent_average_price;
                   _pending_state->market_history[old_key] = old_record;
                 }
               }
               else
                 _pending_state->market_history[old_key] = new_record;

               fc::time_point_sec start_of_this_day = timestamp - (timestamp.sec_since_epoch() % (60*60*24));
               old_key = market_history_key(_quote_id, _base_id, market_history_key::each_day, start_of_this_day);
               if( auto opt = _db_impl._market_history_db.fetch_optional(old_key) )
               {
                 auto old_record = *opt;
                 old_record.volume += new_record.volume;
                 if( new_record.highest_bid > old_record.highest_bid || new_record.lowest_ask < old_record.lowest_ask )
                 {
                   old_record.highest_bid = std::max(new_record.highest_bid, old_record.highest_bid);
                   old_record.lowest_ask = std::min(new_record.lowest_ask, old_record.lowest_ask);
                   old_record.recent_average_price = new_record.recent_average_price;
                   _pending_state->market_history[old_key] = old_record;
                 }
               }
               else
                 _pending_state->market_history[old_key] = new_record;
             }
      }

      pending_chain_state_ptr     _pending_state;
      pending_chain_state_ptr     _prior_state;
      chain_database_impl&        _db_impl;

      optional<market_order>      _current_bid;
      optional<market_order>      _current_ask;
      share_type                  _current_payoff_balance;
      asset_id_type               _quote_id;
      asset_id_type               _base_id;
      market_status               _market_stat;

      int                         _orders_filled;

      vector<market_transaction>  _market_transactions;

      bts::db::cached_level_map< market_index_key, order_record >::iterator       _bid_itr;
      bts::db::cached_level_map< market_index_key, order_record >::iterator       _ask_itr;
      bts::db::cached_level_map< market_index_key, order_record >::iterator       _short_itr;
      bts::db::cached_level_map< market_index_key, collateral_record >::iterator  _collateral_itr;
};
