#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/market_engine_v3.hpp>

#include <algorithm>

#define BTS_BLOCKCHAIN_INITIAL_SHARES (BTS_BLOCKCHAIN_MAX_SHARES/5)
#define BTS_BLOCKCHAIN_MARKET_DEPTH_REQUIREMENT_V3 (BTS_BLOCKCHAIN_INITIAL_SHARES/4000)

namespace bts { namespace blockchain { namespace detail {

      market_engine_v3::market_engine_v3( const pending_chain_state_ptr ps, const chain_database_impl& cdi )
      :_pending_state(ps),_db_impl(cdi),_orders_filled(0)
      {
          _pending_state = std::make_shared<pending_chain_state>( ps );
          _prior_state = ps;
      }

      bool market_engine_v3::execute( asset_id_type quote_id, asset_id_type base_id, const fc::time_point_sec timestamp )
      {
         try {
             const auto pending_block_num = _pending_state->get_head_block_num();

             _quote_id = quote_id;
             _base_id = base_id;
             auto quote_asset = _pending_state->get_asset_record( _quote_id );
             auto base_asset = _pending_state->get_asset_record( _base_id );

             // the order book is sorted from low to high price, so to get the last item (highest bid), we need to go to the first item in the
             // next market class and then back up one
             auto next_pair  = base_id+1 == quote_id ? price( 0, quote_id+1, 0) : price( 0, quote_id, base_id+1 );
             _bid_itr        = _db_impl._bid_db.lower_bound( market_index_key( next_pair ) );
             _ask_itr        = _db_impl._ask_db.lower_bound( market_index_key( price( 0, quote_id, base_id) ) );
             _short_itr      = _db_impl._short_db.lower_bound( market_index_key( next_pair ) );
             _collateral_itr = _db_impl._collateral_db.lower_bound( market_index_key( next_pair ) );

             if( !_ask_itr.valid() )
             {
                _ask_itr = _db_impl._ask_db.begin();
             }

             if( _short_itr.valid() ) --_short_itr;
             else _short_itr = _db_impl._short_db.last();

             if( _bid_itr.valid() )   --_bid_itr;
             else _bid_itr = _db_impl._bid_db.last();

             if( _collateral_itr.valid() )   --_collateral_itr;
             else _collateral_itr = _db_impl._collateral_db.last();

            asset base_volume( 0, base_id );
            asset quote_volume( 0, quote_id );

             ostatus_record market_stat = _pending_state->get_status_record( status_index{ _quote_id, _base_id } );
             if( !market_stat ) market_stat = status_record( quote_id, base_id );

             price max_short_bid;
             price min_cover_ask;
              price opening_price, closing_price, highest_price, lowest_price;

             // while bootstraping we use this metric
             oprice median_price;
             if( quote_asset->is_market_issued() )
                 median_price = _db_impl.self->get_active_feed_price( quote_id );

             // convert any fees collected in quote unit to XTS
             if( base_id == 0 )
             {
                if( quote_asset->is_market_issued() )
                {
                   if( market_stat->center_price.ratio == fc::uint128_t() )
                   {
                      if( !median_price )
                      {
                         FC_CAPTURE_AND_THROW( insufficient_feeds, (quote_id) );
                      }
                      else
                      {
                         market_stat->center_price = *median_price;
                      }
                   }
                   min_cover_ask = market_stat->minimum_ask();

                   if( median_price )
                      max_short_bid = *median_price;
                   else
                      max_short_bid = market_stat->center_price;
                }
                else // we only liquidate fees collected for user issued assets
                {

                  get_next_bid(); // this is necessary for get_next_ask to work with collateral
                  while( get_next_ask() )
                  {
                     if( (asset(quote_asset->collected_fees,quote_id) * _current_ask->get_price()).amount < (10000 * BTS_BLOCKCHAIN_PRECISION) )
                        break;
                     market_transaction mtrx;
                     mtrx.bid_index.order_price = _current_ask->get_price();
                     mtrx.ask_index.order_price = _current_ask->get_price();
                     mtrx.bid_index.owner = address();
                     mtrx.ask_index.owner = _current_ask->get_owner();
                     mtrx.bid_type  = bid_order;
                     mtrx.ask_type  = _current_ask->type;

                     auto ask_quote_quantity = _current_ask->get_quote_quantity();
                     auto quote_quantity_usd = std::min( quote_asset->collected_fees, ask_quote_quantity.amount );
                     mtrx.ask_received = asset(quote_quantity_usd,quote_id);
                     mtrx.ask_paid     = mtrx.ask_received * mtrx.ask_index.order_price;
                     mtrx.bid_paid     = mtrx.ask_received;
                     mtrx.bid_received = mtrx.ask_paid; // these get directed to accumulated fees

                     if( mtrx.ask_paid.amount == 0 )
                        break;

                    push_market_transaction( mtrx );

                    base_volume += mtrx.bid_received;
                    quote_volume += mtrx.ask_received;
                    if( opening_price == price() )
                      opening_price = mtrx.bid_index.order_price;
                    closing_price = mtrx.bid_index.order_price;
                    // Remark: only prices of matched orders be updated to market history
                    // TODO check here: since the orders have been sorted, maybe don't need the 2nd comparison
                    if( highest_price == price() || highest_price < mtrx.bid_index.order_price)
                      highest_price = mtrx.bid_index.order_price;
                    // TODO check here: store lowest ask price or lowest bid price?
                    if( lowest_price == price() || lowest_price > mtrx.ask_index.order_price)
                      lowest_price = mtrx.ask_index.order_price;

                     if( mtrx.ask_type == ask_order )
                        pay_current_ask( mtrx, *base_asset );
                     else
                        pay_current_cover( mtrx, *quote_asset, *base_asset );

                     market_stat->ask_depth -= mtrx.ask_paid.amount;

                     quote_asset->collected_fees -= mtrx.bid_paid.amount;
                     base_asset->collected_fees += mtrx.ask_paid.amount;
                  }
                }
             }

             int last_orders_filled = -1;

             bool order_did_execute = false;
             while( get_next_bid() && get_next_ask() )
             {
                // make sure that at least one order was matched
                // every time we enter the loop.
                if( _orders_filled == last_orders_filled )
                {
                   FC_ASSERT( _orders_filled != last_orders_filled, "we appear caught in a order matching loop" );
                }
                last_orders_filled = _orders_filled;

                auto bid_quantity_xts = _current_bid->get_quantity();
                auto ask_quantity_xts = _current_ask->get_quantity();

                asset xts_paid_by_short( 0, base_id );
                asset current_bid_balance  = _current_bid->get_balance();
                asset current_ask_balance  = _current_ask->get_balance();

                /** the market transaction we are filling out */
                market_transaction mtrx;
                mtrx.bid_index.order_price = _current_bid->get_price();
                mtrx.ask_index.order_price = _current_ask->get_price();
                mtrx.bid_index.owner = _current_bid->get_owner();
                mtrx.ask_index.owner = _current_ask->get_owner();
                mtrx.bid_type  = _current_bid->type;
                mtrx.ask_type  = _current_ask->type;

                if( _current_ask->type == cover_order && _current_bid->type == short_order )
                {
                   FC_ASSERT( quote_asset->is_market_issued() && base_id == 0 );
                   if( mtrx.ask_index.order_price < mtrx.bid_index.order_price ) // the call price has not been reached
                      break;

                   /**
                    *  Don't allow shorts to be executed if they are too far over priced or they will be
                    *  immediately under collateralized.
                    */
                   if( mtrx.bid_index.order_price > market_stat->maximum_bid() )
                   {
                      _current_bid.reset();
                      continue;
                   }
                   /**
                    *  Don't allow shorts to be executed if they are too far over priced or they will be
                    *  immediately under collateralized.
                    */
                   if( mtrx.bid_index.order_price < market_stat->minimum_ask() )
                   {
                      _current_ask.reset();
                      continue;
                   }

                   mtrx.ask_index.order_price = mtrx.bid_index.order_price;

                   // we want to sell enough XTS to cover our balance.
                   ask_quantity_xts  = current_ask_balance * mtrx.bid_index.order_price;

                   if( ask_quantity_xts.amount > *_current_ask->collateral )
                      ask_quantity_xts.amount = *_current_ask->collateral;

                   auto quantity_xts = std::min( bid_quantity_xts, ask_quantity_xts );

                   if( quantity_xts == ask_quantity_xts )
                        mtrx.bid_paid      = current_ask_balance;
                   else
                        mtrx.bid_paid      = quantity_xts * mtrx.bid_index.order_price;

                   mtrx.ask_received  = mtrx.bid_paid;
                   xts_paid_by_short  = quantity_xts;

                   // rounding errors go into collateral, round to the nearest 1 XTS
                   if( bid_quantity_xts.amount - xts_paid_by_short.amount < BTS_BLOCKCHAIN_PRECISION )
                      xts_paid_by_short = bid_quantity_xts;

                   mtrx.ask_paid       = quantity_xts;
                   mtrx.bid_received   = quantity_xts;

                   // the short always pays the quantity.

                   FC_ASSERT( xts_paid_by_short <= current_bid_balance );
                   FC_ASSERT( mtrx.ask_paid.amount <= *_current_ask->collateral );

                   order_did_execute = true;
                   pay_current_short( mtrx, xts_paid_by_short, *quote_asset );
                   pay_current_cover( mtrx, *quote_asset, *base_asset );

                   market_stat->bid_depth -= xts_paid_by_short.amount;
                   market_stat->ask_depth += xts_paid_by_short.amount;
                   market_stat->ask_depth -= mtrx.ask_paid.amount;
                }
                else if( _current_ask->type == cover_order && _current_bid->type == bid_order )
                {
                   FC_ASSERT( quote_asset->is_market_issued() && base_id == 0 );
                   if( mtrx.ask_index.order_price < mtrx.bid_index.order_price )
                      break; // the call price has not been reached

                   /**
                    *  Don't allow margin calls to be executed too far below
                    *  the minimum ask, this could lead to an attack where someone
                    *  walks the whole book to steal the collateral.
                    */
                   if( mtrx.bid_index.order_price < market_stat->minimum_ask() )
                   {
                      _current_ask.reset();
                      continue;
                   }

                   mtrx.ask_index.order_price = mtrx.bid_index.order_price;

                   auto max_usd_purchase = asset(*_current_ask->collateral,0) * mtrx.bid_index.order_price;
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
                      mtrx.ask_paid = usd_exchanged * mtrx.bid_index.order_price;

                   mtrx.bid_received = mtrx.ask_paid;


                   market_stat->ask_depth -= mtrx.ask_paid.amount;
                   order_did_execute = true;
                   pay_current_bid( mtrx, *quote_asset );
                   pay_current_cover( mtrx, *quote_asset, *base_asset );
                }
                else if( _current_ask->type == ask_order && _current_bid->type == short_order )
                {
                   if( mtrx.bid_index.order_price < mtrx.ask_index.order_price ) break;
                   FC_ASSERT( quote_asset->is_market_issued() && base_id == 0 );

                   /**
                    *  If the ask is less than the "max short bid" then that means the
                    *  ask (those with XTS wanting to buy USD) are willing to accept
                    *  a price lower than the median.... this will generally mean that
                    *  everyone else with USD looking to sell below parity has been
                    *  bought out and the buyers of USD are willing to pay above parity.
                    */
                   if( mtrx.bid_index.order_price > max_short_bid && mtrx.ask_index.order_price > max_short_bid )
                   {
                      _current_bid.reset();
                      continue;
                   }

                   /**
                    *  Don't allow shorts to be executed if they are too far over priced or they will be
                    *  immediately under collateralized.
                    */
                   if( mtrx.bid_index.order_price > market_stat->maximum_bid() )
                   {
                      _current_bid.reset();
                      continue;
                   }

                   auto quantity_xts   = std::min( bid_quantity_xts, ask_quantity_xts );

                   mtrx.bid_paid       = quantity_xts * mtrx.bid_index.order_price;
                   mtrx.ask_paid       = quantity_xts;
                   mtrx.bid_received   = mtrx.ask_paid;
                   mtrx.ask_received   = mtrx.ask_paid * mtrx.ask_index.order_price;

                   xts_paid_by_short   = quantity_xts; //bid_quantity_xts;

                   // rounding errors go into collateral, round to the nearest 1 XTS
                   if( bid_quantity_xts.amount - quantity_xts.amount < BTS_BLOCKCHAIN_PRECISION )
                      xts_paid_by_short = bid_quantity_xts;


                   FC_ASSERT( xts_paid_by_short <= _current_bid->get_balance() );
                   order_did_execute = true;
                   pay_current_short( mtrx, xts_paid_by_short, *quote_asset );
                   pay_current_ask( mtrx, *base_asset );

                   market_stat->bid_depth -= xts_paid_by_short.amount;
                   market_stat->ask_depth += xts_paid_by_short.amount;

                   mtrx.quote_fees = mtrx.bid_paid - mtrx.ask_received;
                }
                else if( _current_ask->type == ask_order && _current_bid->type == bid_order )
                {
                   if( mtrx.bid_index.order_price < mtrx.ask_index.order_price ) break;
                   auto quantity_xts = std::min( bid_quantity_xts, ask_quantity_xts );

                   mtrx.bid_paid       = quantity_xts * mtrx.bid_index.order_price;
                   // ask gets exactly what they asked for
                   mtrx.ask_received   = quantity_xts * mtrx.ask_index.order_price;
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
                   mtrx.quote_fees = mtrx.bid_paid - mtrx.ask_received;
                }

                push_market_transaction(mtrx);
                base_volume += mtrx.bid_received;
                quote_volume += mtrx.ask_received;
                if( opening_price == price() )
                  opening_price = mtrx.bid_index.order_price;
                closing_price = mtrx.bid_index.order_price;

            quote_asset->collected_fees += mtrx.quote_fees.amount;
            base_asset->collected_fees += mtrx.base_fees.amount;
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
                   market_stat->center_price = *median_price;
                }
                else
                {
                   // after the market is running solid we can use this metric...
                   market_stat->center_price.ratio *= (BTS_BLOCKCHAIN_BLOCKS_PER_HOUR-1);

                   const auto max_bid = market_stat->maximum_bid();

                   // limit the maximum movement rate of the price.
                   if( _current_bid->get_price() < min_cover_ask )
                      market_stat->center_price.ratio += min_cover_ask.ratio;
                   else if( _current_bid->get_price() > max_bid )
                      market_stat->center_price.ratio += max_bid.ratio; //max_short_bid.ratio;
                   else
                      market_stat->center_price.ratio += _current_bid->get_price().ratio;

                   if( _current_ask->get_price() < min_cover_ask )
                      market_stat->center_price.ratio += min_cover_ask.ratio;
                   else if( _current_ask->get_price() > max_bid )
                      market_stat->center_price.ratio += max_bid.ratio;
                   else
                      market_stat->center_price.ratio += _current_ask->get_price().ratio;

                   market_stat->center_price.ratio /= (BTS_BLOCKCHAIN_BLOCKS_PER_HOUR+1);
                }
             }

             if( quote_asset->is_market_issued() && base_id == 0 )
             {
                if( market_stat->ask_depth < BTS_BLOCKCHAIN_MARKET_DEPTH_REQUIREMENT_V3
                    || market_stat->bid_depth < BTS_BLOCKCHAIN_MARKET_DEPTH_REQUIREMENT_V3 )
                {
                   _market_transactions.clear(); // nothing should have executed
                  std::string reason = "After executing orders there was insufficient depth remaining";
                  FC_CAPTURE_AND_THROW( insufficient_depth, (reason)(market_stat)(BTS_BLOCKCHAIN_MARKET_DEPTH_REQUIREMENT_V3) );
                }
             }
             _pending_state->store_status_record( *market_stat );

              // Remark: only prices of matched orders be updated to market history
            update_market_history( base_volume, quote_volume, highest_price, lowest_price,
                opening_price, closing_price, timestamp );

             _pending_state->apply_changes();
             return true;
        }
        catch( const fc::exception& e )
        {
           auto market_state = _prior_state->get_status_record( status_index{ quote_id, base_id } );
           if( !market_state )
              market_state = status_record( quote_id, base_id );
           market_state->last_error = e;
           _prior_state->store_status_record( *market_state );
        }
        return false;
      } // execute(...)

      void market_engine_v3::push_market_transaction( const market_transaction& mtrx )
      { try {
          FC_ASSERT( mtrx.bid_paid.amount >= 0 );
          FC_ASSERT( mtrx.ask_paid.amount >= 0 );
          FC_ASSERT( mtrx.bid_received.amount >= 0 );
          FC_ASSERT( mtrx.ask_received .amount>= 0 );
          FC_ASSERT( mtrx.bid_paid >= mtrx.ask_received );
          FC_ASSERT( mtrx.ask_paid >= mtrx.bid_received );

          _market_transactions.push_back(mtrx);
      } FC_CAPTURE_AND_RETHROW( (mtrx) ) }

      void market_engine_v3::pay_current_short(const market_transaction& mtrx, const asset& xts_paid_by_short, asset_record& quote_asset )
      { try {
          FC_ASSERT( _current_bid->type == short_order );
          FC_ASSERT( mtrx.bid_type == short_order );

          /** NOTE: the short may pay extra XTS to the collateral in the event rounding occurs.  This
           * just checks to make sure it is reasonable.
           */
          FC_ASSERT( mtrx.ask_paid <= xts_paid_by_short, "", ("mtrx",mtrx)("xts_paid_by_short",xts_paid_by_short) );
          /** sanity check to make sure that the only difference is rounding error */
          FC_ASSERT( mtrx.ask_paid.amount >= (xts_paid_by_short.amount - BTS_BLOCKCHAIN_PRECISION), "", ("mtrx",mtrx)("xts_paid_by_short",xts_paid_by_short) );

          quote_asset.current_supply += mtrx.bid_paid.amount;

          auto collateral  = xts_paid_by_short + mtrx.ask_paid;
          if( mtrx.bid_paid.amount <= 0 ) // WHY is this ever negitive??
          {
             FC_ASSERT( mtrx.bid_paid.amount >= 0 );
             _current_bid->state.balance -= xts_paid_by_short.amount;
             return;
          }

          auto cover_price = mtrx.bid_index.order_price;
          cover_price.ratio *= 3;
          cover_price.ratio /= 4;
         // auto cover_price = mtrx.bid_paid / asset( (3*collateral.amount)/4, _base_id );

          market_index_key cover_index( cover_price, _current_bid->get_owner() );
          auto ocover_record = _pending_state->get_collateral_record( cover_index );

          if( NOT ocover_record ) ocover_record = collateral_record();

          ocover_record->collateral_balance += collateral.amount;
          ocover_record->payoff_balance += mtrx.bid_paid.amount;
          ocover_record->interest_rate = price( 0, quote_asset.id, 0 );
          ocover_record->expiration = fc::time_point( _pending_state->now() ) + fc::days( 365 );

          FC_ASSERT( ocover_record->payoff_balance >= 0, "", ("record",ocover_record) );
          FC_ASSERT( ocover_record->collateral_balance >= 0 , "", ("record",ocover_record));

          _current_bid->state.balance -= xts_paid_by_short.amount;
          FC_ASSERT( _current_bid->state.balance >= 0 );

          _pending_state->store_collateral_record( cover_index, *ocover_record );

          _pending_state->store_short_record( _current_bid->market_index, _current_bid->state );
      } FC_CAPTURE_AND_RETHROW( (mtrx)  ) }

      void market_engine_v3::pay_current_bid( const market_transaction& mtrx, asset_record& quote_asset )
      { try {
          FC_ASSERT( _current_bid->type == bid_order );
          FC_ASSERT( mtrx.bid_type == bid_order );
          _current_bid->state.balance -= mtrx.bid_paid.amount;
          FC_ASSERT( _current_bid->state.balance >= 0 );

          auto bid_payout = _pending_state->get_balance_record(
                                    withdraw_condition( withdraw_with_signature(mtrx.bid_index.owner), _base_id ).get_address() );
          if( !bid_payout )
             bid_payout = balance_record( mtrx.bid_index.owner, asset(0,_base_id), 0 );

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

      void market_engine_v3::pay_current_cover( market_transaction& mtrx, asset_record& quote_asset, asset_record& base_asset )
      { try {
          FC_ASSERT( _current_ask->type == cover_order );
          FC_ASSERT( mtrx.ask_type == cover_order );

          // we are in the margin call range...
          _current_ask->state.balance  -= mtrx.bid_paid.amount;
          *(_current_ask->collateral)  -= mtrx.ask_paid.amount;

          FC_ASSERT( _current_ask->state.balance >= 0 );
          FC_ASSERT( *_current_ask->collateral >= 0, "", ("mtrx",mtrx)("_current_ask", _current_ask)  );

          quote_asset.current_supply -= mtrx.ask_received.amount;
          if( *_current_ask->collateral == 0 )
          {
              quote_asset.collected_fees -= _current_ask->state.balance;
              _current_ask->state.balance = 0;
          }


          if( _current_ask->state.balance == 0 && *_current_ask->collateral > 0 ) // no more USD left
          { // send collateral home to mommy & daddy
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

                // these go to the network... as dividends..
                mtrx.base_fees  += asset(fee,0);

                base_asset.collected_fees += fee;

                ask_payout->balance += left_over_collateral;
                ask_payout->last_update = _pending_state->now();
                ask_payout->deposit_date = _pending_state->now();

                mtrx.returned_collateral = left_over_collateral;

                _pending_state->store_balance_record( *ask_payout );
                _current_ask->collateral = 0;
          }

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

      void market_engine_v3::pay_current_ask( const market_transaction& mtrx, asset_record& base_asset )
      { try {
          FC_ASSERT( _current_ask->type == ask_order ); // update ask + payout

          _current_ask->state.balance -= mtrx.ask_paid.amount;
          FC_ASSERT( _current_ask->state.balance >= 0 );

          auto ask_balance_address = withdraw_condition( withdraw_with_signature(mtrx.ask_index.owner), _quote_id ).get_address();
          auto ask_payout = _pending_state->get_balance_record( ask_balance_address );
          if( !ask_payout )
             ask_payout = balance_record( mtrx.ask_index.owner, asset(0,_quote_id), 0 );
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

      bool market_engine_v3::get_next_bid()
      { try {
         if( _current_bid && _current_bid->get_quantity().amount > 0 )
            return _current_bid.valid();

         ++_orders_filled;
         _current_bid.reset();
         if( _bid_itr.valid() )
         {
            auto bid = market_order( bid_order, _bid_itr.key(), _bid_itr.value() );
            if( bid.get_price().quote_asset_id == _quote_id &&
                bid.get_price().base_asset_id == _base_id )
            {
                _current_bid = bid;
            }
         }

         if( _short_itr.valid() )
         {
            auto bid = market_order( short_order, _short_itr.key(), _short_itr.value() );
            if( bid.get_price().quote_asset_id == _quote_id &&
                bid.get_price().base_asset_id == _base_id )
            {
                if( !_current_bid || _current_bid->get_price() < bid.get_price() )
                {
                   --_short_itr;
                   _current_bid = bid;
                   return _current_bid.valid();
                }
            }
         }
         else
         {
         }
         if( _bid_itr.valid() ) --_bid_itr;
         return _current_bid.valid();
      } FC_CAPTURE_AND_RETHROW() }

      bool market_engine_v3::get_next_ask()
      { try {
         if( _current_ask && _current_ask->state.balance > 0 )
         {
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
                                           order_record( _collateral_itr.value().payoff_balance ),
                                           _collateral_itr.value().collateral_balance,
                                           price() );

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
                      --_collateral_itr;
                      return _current_ask.valid();
                   }
                }
            }
            break;
         }

         if( _ask_itr.valid() )
         {
            auto ask = market_order( ask_order, _ask_itr.key(), _ask_itr.value() );
            if( ask.get_price().quote_asset_id == _quote_id &&
                ask.get_price().base_asset_id == _base_id )
            {
                _current_ask = ask;
            }
            ++_ask_itr;
            return _current_ask.valid();
         }
         return _current_ask.valid();
      } FC_CAPTURE_AND_RETHROW() }


      /**
       *  This method should not affect market execution or validation and
       *  is for historical purposes only.
       */
  void market_engine_v3::update_market_history( const asset& base_volume,
                                               const asset& quote_volume,
                                               const price& highest_price,
                                               const price& lowest_price,
                                               const price& opening_price,
                                               const price& closing_price,
                                               const fc::time_point_sec timestamp )
  {
          // Remark: only prices of matched orders be updated to market history
            if( base_volume.amount == 0 && quote_volume.amount == 0)
                return;

            market_history_key key(_quote_id, _base_id, market_history_key::each_block, _db_impl._head_block_header.timestamp);
            market_history_record new_record( highest_price, lowest_price, opening_price, closing_price,
                base_volume.amount, quote_volume.amount );

            //LevelDB iterators are dumb and don't support proper past-the-end semantics.
            auto last_key_itr = _db_impl._market_history_db.lower_bound(key);
            if( !last_key_itr.valid() )
              last_key_itr = _db_impl._market_history_db.last();
            else
              --last_key_itr;

            key.timestamp = timestamp;

            //Unless the previous record for this market is the same as ours...
            // TODO check here: the previous commit checks for volume and prices change here,
            //                  I replaced them with key comparison, but looks odd as well.
            //                  maybe need to remove the judgements at all? since volume info is
            //                  always needed to be updated to market history,
            //                  even if prices and volumes are same to last block.
            if( (!(last_key_itr.valid()
                && last_key_itr.key() == key)) )
            {
              //...add a new entry to the history table.
              _pending_state->market_history[key] = new_record;
            }

               fc::time_point_sec start_of_this_hour = timestamp - (timestamp.sec_since_epoch() % (60*60));
               market_history_key old_key(_quote_id, _base_id, market_history_key::each_hour, start_of_this_hour);
               if( auto opt = _db_impl._market_history_db.fetch_optional(old_key) )
               {
                 auto old_record = *opt;
                old_record.base_volume += new_record.base_volume;
                old_record.quote_volume += new_record.quote_volume;
                 if( new_record.highest_bid > old_record.highest_bid || new_record.lowest_ask < old_record.lowest_ask )
                 {
                   old_record.highest_bid = std::max(new_record.highest_bid, old_record.highest_bid);
                   old_record.lowest_ask = std::min(new_record.lowest_ask, old_record.lowest_ask);
                  }
                  // always update old data since volume changed
                  _pending_state->market_history[old_key] = old_record;
               }
               else
                 _pending_state->market_history[old_key] = new_record;

               fc::time_point_sec start_of_this_day = timestamp - (timestamp.sec_since_epoch() % (60*60*24));
               old_key = market_history_key(_quote_id, _base_id, market_history_key::each_day, start_of_this_day);
               if( auto opt = _db_impl._market_history_db.fetch_optional(old_key) )
               {
                 auto old_record = *opt;
                old_record.base_volume += new_record.base_volume;
                old_record.quote_volume += new_record.quote_volume;
                 if( new_record.highest_bid > old_record.highest_bid || new_record.lowest_ask < old_record.lowest_ask )
                 {
                   old_record.highest_bid = std::max(new_record.highest_bid, old_record.highest_bid);
                   old_record.lowest_ask = std::min(new_record.lowest_ask, old_record.lowest_ask);
                  }
                  // always update old data since volume changed
                  _pending_state->market_history[old_key] = old_record;
               }
               else
                 _pending_state->market_history[old_key] = new_record;
      }

} } } // end namespace bts::blockchain::detail

#undef BTS_BLOCKCHAIN_MARKET_DEPTH_REQUIREMENT_V3
