/** This file is designed to be included in detail::chain_database_impl, but is pulled into a separate file for
 * ease of maintenance and upgrade.
 */
// This is used to save & restore the _current_ask with its previous value, required to
// reproduce some quirky behavior in pre-BTSX_MARKET_FORK_4_BLOCK_NUM blocks.
struct save_and_restore_ask
{
  fc::optional<market_order>& _current_ask_to_swap;
  bool _swapped;
  fc::optional<market_order> _original_current_ask;
  save_and_restore_ask(fc::optional<market_order>& current_ask, const fc::optional<market_order>& backup_ask) :
    _current_ask_to_swap(current_ask),
    _swapped(false)
  {
    if (!_current_ask_to_swap)
      {
      _swapped = true;
      _original_current_ask = _current_ask_to_swap;
      _current_ask_to_swap = backup_ask ? *backup_ask : market_order();
      }
  }
  ~save_and_restore_ask()
  {
    if (_swapped)
      _current_ask_to_swap = _original_current_ask;
  }
};

class market_engine_v2
{
   public:
      market_engine_v2( pending_chain_state_ptr ps, chain_database_impl& cdi )
      :_pending_state(ps),_db_impl(cdi),_orders_filled(0)
      {
          _pending_state = std::make_shared<pending_chain_state>( ps );
          _prior_state = ps;
      }

      void execute( asset_id_type quote_id, asset_id_type base_id, const fc::time_point_sec& timestamp )
      {
         try {
             const auto pending_block_num = _pending_state->get_head_block_num();

             _quote_id = quote_id;
             _base_id = base_id;
             oasset_record quote_asset = _pending_state->get_asset_record( _quote_id );
             oasset_record base_asset = _pending_state->get_asset_record( _base_id );

             // the order book is sorted from low to high price, so to get the last item (highest bid), we need to go to the first item in the
             // next market class and then back up one
             price next_pair = base_id+1 == quote_id ? price( 0, quote_id+1, 0) : price( 0, quote_id, base_id+1 );
             _bid_itr        = _db_impl._bid_db.lower_bound( market_index_key( next_pair ) );
             _ask_itr        = _db_impl._ask_db.lower_bound( market_index_key( price( 0, quote_id, base_id) ) );
             _short_itr      = _db_impl._short_db.lower_bound( market_index_key( next_pair ) );
             _collateral_itr = _db_impl._collateral_db.lower_bound( market_index_key( next_pair ) );

             if( !_ask_itr.valid() )
             {
                wlog( "ask iter invalid..." );
                _ask_itr = _db_impl._ask_db.begin();
             }

             if( _short_itr.valid() ) --_short_itr;
             else _short_itr = _db_impl._short_db.last();

             if( _bid_itr.valid() )   --_bid_itr;
             else _bid_itr = _db_impl._bid_db.last();

             if( _collateral_itr.valid() )   --_collateral_itr;
             else _collateral_itr = _db_impl._collateral_db.last();

             asset trading_volume(0, base_id);

             omarket_status market_stat = _pending_state->get_market_status( _quote_id, _base_id );
             if( !market_stat ) market_stat = market_status( quote_id, base_id, 0, 0 );

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
                      }
                   }
                   max_short_bid = market_stat->maximum_bid();
                   min_cover_ask = market_stat->minimum_ask();

                   if( pending_block_num >= BTSX_MARKET_FORK_4_BLOCK_NUM )
                   {
                      if( median_price )
                         max_short_bid = *median_price;
                      else
                         max_short_bid = market_stat->avg_price_1h;
                   }
                }
                else // we only liquidate fees collected for user issued assets
                {
  //  wlog( "==========================  LIQUIDATE FEES ${amount}  =========================\n", ("amount", quote_asset->collected_fees) );

                  get_next_bid(); // this is necessary for get_next_ask to work with collateral
                  while( get_next_ask() )
                  {
                     assert(_pending_state->get_head_block_num() < BTSX_MARKET_FORK_4_BLOCK_NUM || _current_ask);
                     if (_pending_state->get_head_block_num() >= BTSX_MARKET_FORK_4_BLOCK_NUM && !_current_ask)
                       FC_THROW_EXCEPTION(evaluation_error, "no current_ask"); // should never happen, but if it does, don't swap in the backup ask
                     save_and_restore_ask current_ask_swapper(_current_ask, _current_ask_backup);

                     if( (asset(quote_asset->collected_fees,quote_id) * _current_ask->get_price()).amount < (10000 * BTS_BLOCKCHAIN_PRECISION) )
                        break;
                //     idump( (_current_ask) );
                     market_transaction mtrx;
                     mtrx.bid_price = _current_ask->get_price();
                     mtrx.ask_price = _current_ask->get_price();
                     mtrx.bid_owner = address();
                     mtrx.ask_owner = _current_ask->get_owner();
                     mtrx.bid_type  = bid_order;
                     mtrx.ask_type  = _current_ask->type;

                     auto ask_quote_quantity = _current_ask->get_quote_quantity();
                     auto quote_quantity_usd = std::min( quote_asset->collected_fees, ask_quote_quantity.amount );
                     mtrx.ask_received = asset(quote_quantity_usd,quote_id);
                     mtrx.ask_paid     = mtrx.ask_received * mtrx.ask_price;
                     mtrx.bid_paid     = mtrx.ask_received;
                     mtrx.bid_received = mtrx.ask_paid; // these get directed to accumulated fees

                     // mtrx.fees_collected = mtrx.ask_paid;

                     if( mtrx.ask_paid.amount == 0 )
                        break;

                     push_market_transaction(mtrx);
                     if( mtrx.ask_received.asset_id == 0 )
                       trading_volume += mtrx.ask_received;
                     else if( mtrx.bid_received.asset_id == 0 )
                       trading_volume += mtrx.bid_received;
                     if( opening_price == price() )
                       opening_price = mtrx.bid_price;
                     closing_price = mtrx.bid_price;

                     if( mtrx.ask_type == ask_order )
                        pay_current_ask( mtrx, *base_asset );
                     else
                        pay_current_cover( mtrx, *quote_asset );

                     market_stat->ask_depth -= mtrx.ask_paid.amount;

                     quote_asset->collected_fees -= mtrx.bid_paid.amount;
                     _pending_state->store_asset_record(*quote_asset);
                     _pending_state->store_asset_record(*base_asset);

                     auto prev_accumulated_fees = _pending_state->get_accumulated_fees();
                     _pending_state->set_accumulated_fees( prev_accumulated_fees + mtrx.ask_paid.amount );
                  }
                //  wlog( "==========================  DONE LIQUIDATE FEES BALANCE: ${amount}=========================\n", ("amount", quote_asset->collected_fees) );
                }
             }

             int last_orders_filled = -1;

             bool order_did_execute = false;
             while( get_next_bid() && get_next_ask() )
             {
                assert(_pending_state->get_head_block_num() < BTSX_MARKET_FORK_4_BLOCK_NUM || _current_ask);
                if (_pending_state->get_head_block_num() >= BTSX_MARKET_FORK_4_BLOCK_NUM && !_current_ask)
                  FC_THROW_EXCEPTION(evaluation_error, "no current_ask"); // should never happen, but if it does, don't swap in the backup ask
                save_and_restore_ask current_ask_swapper(_current_ask, _current_ask_backup);

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
                mtrx.bid_price = _current_bid->get_price();
                mtrx.ask_price = _current_ask->get_price();
                mtrx.bid_owner = _current_bid->get_owner();
                mtrx.ask_owner = _current_ask->get_owner();
                mtrx.bid_type  = _current_bid->type;
                mtrx.ask_type  = _current_ask->type;

                if( _current_ask->type == cover_order && _current_bid->type == short_order )
                {
                   //elog( "CURRENT ASK IS COVER" );
                   FC_ASSERT( quote_asset->is_market_issued() && base_id == 0 );
                   if( mtrx.ask_price < mtrx.bid_price ) // the call price has not been reached
                      break;

                   if( pending_block_num < BTSX_MARKET_FORK_4_BLOCK_NUM )
                   {
                      // in the event that there is a margin call, we must accept the
                      // bid price assuming the bid price is reasonable
                      if( mtrx.bid_price < min_cover_ask )
                      {
                         //wlog( "skipping cover ${x} < min_cover_ask ${b}", ("x",_current_ask->get_price())("b", min_cover_ask)  );
                         reset_current_ask();
                         continue;
                      }
                   }

                   if( pending_block_num >= BTSX_MARKET_FORK_5_BLOCK_NUM )
                   {
                       /**
                        *  Don't allow shorts to be executed if they are too far over priced or they will be
                        *  immediately under collateralized.
                        */
                       if( mtrx.bid_price > market_stat->maximum_bid() )
                       {
                          //wlog( "skipping short ${x} < max_short_bid ${b}", ("x",mtrx.bid_price)("b", max_short_bid)  );
                          // TODO: cancel the short order...
                          _current_bid.reset();
                          continue;
                       }
                       /**
                        *  Don't allow shorts to be executed if they are too far over priced or they will be
                        *  immediately under collateralized.
                        */
                       if( mtrx.bid_price < market_stat->minimum_ask() )
                       {
                          //wlog( "skipping short ${x} < max_short_bid ${b}", ("x",mtrx.bid_price)("b", max_short_bid)  );
                          // TODO: cancel the short order...
                          reset_current_ask();
                          continue;
                       }
                   }
                   else
                   {
                       if( mtrx.bid_price > max_short_bid )
                       {
                          //wlog( "skipping short ${x} < max_short_bid ${b}", ("x",mtrx.bid_price)("b", max_short_bid)  );
                          // TODO: cancel the short order...
                          _current_bid.reset();
                          continue;
                       }
                   }
                   mtrx.ask_price = mtrx.bid_price;

                   // we want to sell enough XTS to cover our balance.
                   //ulog("Current ask balance:  ${ask_balance},  current bid price: ${bid_price}",
                   //     ("ask_balance", current_ask_balance)("bid_price", mtrx.bid_price));
                   ask_quantity_xts  = current_ask_balance * mtrx.bid_price;

                   /*
                   ulog("Current bid:  ${bid} \n  Current ask: ${ask}",
                           ("bid", _current_bid)
                           ("ask", _current_ask)
                           ("ask_quantity_xts",ask_quantity_xts));
                           */
                   if( ask_quantity_xts.amount > *_current_ask->collateral )
                      ask_quantity_xts.amount = *_current_ask->collateral;

                   auto quantity_xts = std::min( bid_quantity_xts, ask_quantity_xts );

                   if( quantity_xts == ask_quantity_xts )
                        mtrx.bid_paid      = current_ask_balance;
                   else
                        mtrx.bid_paid      = quantity_xts * mtrx.bid_price;
                   //ulog( "mtrx: ${mtrx}", ("mtrx",mtrx) );

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
                   pay_current_cover( mtrx, *quote_asset );

                   market_stat->bid_depth -= xts_paid_by_short.amount;
                   market_stat->ask_depth += xts_paid_by_short.amount;
                   market_stat->ask_depth -= mtrx.ask_paid.amount;
                }
                else if( _current_ask->type == cover_order && _current_bid->type == bid_order )
                {
                   //elog( "CURRENT ASK IS COVER" );
                   FC_ASSERT( quote_asset->is_market_issued() && base_id == 0 );
                   if( mtrx.ask_price < mtrx.bid_price )
                      break; // the call price has not been reached

                   if( pending_block_num >= BTSX_MARKET_FORK_5_BLOCK_NUM )
                   {
                       /**
                        *  Don't allow margin calls to be executed too far below
                        *  the minimum ask, this could lead to an attack where someone
                        *  walks the whole book to steal the collateral.
                        */
                       if( mtrx.bid_price < market_stat->minimum_ask() )
                       {
                          //wlog( "skipping short ${x} < max_short_bid ${b}", ("x",mtrx.bid_price)("b", max_short_bid)  );
                          // TODO: cancel the short order...
                          reset_current_ask();
                          continue;
                       }
                   }

                   mtrx.ask_price = mtrx.bid_price;

                   if( pending_block_num < BTSX_MARKET_FORK_4_BLOCK_NUM )
                   {
                      // in the event that there is a margin call, we must accept the
                      // bid price assuming the bid price is reasonable
                      if( mtrx.bid_price < min_cover_ask )
                      {
                         wlog( "skipping cover ${x} < min_cover_ask ${b}", ("x",_current_ask->get_price())("b", min_cover_ask)  );
                         reset_current_ask();
                         continue;
                      }
                   }

                   auto max_usd_purchase = asset(*_current_ask->collateral,0) * mtrx.bid_price;
                   auto usd_exchanged = std::min( current_bid_balance, max_usd_purchase );

                   if( pending_block_num >= BTSX_MARKET_FORK_3_BLOCK_NUM )
                   {
                      const auto required_usd_purchase = _current_ask->get_balance();
                      if( required_usd_purchase < usd_exchanged )
                         usd_exchanged = required_usd_purchase;
                   }

                   mtrx.bid_paid     = usd_exchanged;
                   mtrx.ask_received = usd_exchanged;

                   // handle rounding errors
                   if( usd_exchanged == max_usd_purchase )
                      mtrx.ask_paid     = asset(*_current_ask->collateral,0);
                   else
                      mtrx.ask_paid = usd_exchanged * mtrx.bid_price;

                   mtrx.bid_received = mtrx.ask_paid;


                   market_stat->ask_depth -= mtrx.ask_paid.amount;
                   order_did_execute = true;
                   pay_current_bid( mtrx, *quote_asset );
                   pay_current_cover( mtrx, *quote_asset );
                }
                else if( _current_ask->type == ask_order && _current_bid->type == short_order )
                {
                   if( mtrx.bid_price < mtrx.ask_price ) break;
                   FC_ASSERT( quote_asset->is_market_issued() && base_id == 0 );

                   if( pending_block_num >= BTSX_MARKET_FORK_5_BLOCK_NUM )
                   {
                       /**
                        *  If the ask is less than the "max short bid" then that means the
                        *  ask (those with XTS wanting to buy USD) are willing to accept
                        *  a price lower than the median.... this will generally mean that
                        *  everyone else with USD looking to sell below parity has been
                        *  bought out and the buyers of USD are willing to pay above parity.
                        */
                       if( mtrx.ask_price <= max_short_bid )
                       {
                          // wlog( "skipping short ${x} < max_short_bid ${b}", ("x",mtrx.bid_price)("b", max_short_bid)  );
                          // TODO: cancel the short order...
                          _current_bid.reset();
                          continue;
                       }

                       /**
                        *  Don't allow shorts to be executed if they are too far over priced or they will be
                        *  immediately under collateralized.
                        */
                       if( mtrx.bid_price > market_stat->maximum_bid() )
                       {
                          // wlog( "skipping short ${x} < max_short_bid ${b}", ("x",mtrx.bid_price)("b", max_short_bid)  );
                          // TODO: cancel the short order...
                          _current_bid.reset();
                          continue;
                       }
                   }
                   else
                   {
                       if( mtrx.bid_price > max_short_bid )
                       {
                          // wlog( "skipping short ${x} < max_short_bid ${b}", ("x",mtrx.bid_price)("b", max_short_bid)  );
                          // TODO: cancel the short order...
                          _current_bid.reset();
                          continue;
                       }
                   }

                   auto quantity_xts   = std::min( bid_quantity_xts, ask_quantity_xts );

                   mtrx.bid_paid       = quantity_xts * mtrx.bid_price;
                   mtrx.ask_paid       = quantity_xts;
                   mtrx.bid_received   = mtrx.ask_paid;
                   mtrx.ask_received   = mtrx.ask_paid * mtrx.ask_price;

                   //ulog( "bid_quat: ${b}  balance ${q}  ask ${a}\n", ("b",quantity_xts)("q",*_current_bid)("a",*_current_ask) );
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

                   mtrx.fees_collected = mtrx.bid_paid - mtrx.ask_received;
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

                accumulate_fees( mtrx, *quote_asset );
             } // while( next bid && next ask )


             // update any fees collected
             _pending_state->store_asset_record( *quote_asset );
             _pending_state->store_asset_record( *base_asset );

             market_stat->last_error.reset();

             if( pending_block_num >= BTSX_MARKET_FORK_3_BLOCK_NUM )
                 order_did_execute |= (pending_block_num % 6) == 0;

             if( _current_bid && _current_ask && order_did_execute )
             {
                if( median_price && pending_block_num >= BTSX_MARKET_FORK_5_BLOCK_NUM )
                {
                   market_stat->avg_price_1h = *median_price;
                }
                else
                {
                   // after the market is running solid we can use this metric...
                   market_stat->avg_price_1h.ratio *= (BTS_BLOCKCHAIN_BLOCKS_PER_HOUR-1);

                   if( pending_block_num >= BTSX_MARKET_FORK_4_BLOCK_NUM )
                   {
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
                   }
                   else
                   {
                      // limit the maximum movement rate of the price.
                      if( _current_bid->get_price() < min_cover_ask )
                         market_stat->avg_price_1h.ratio += min_cover_ask.ratio;
                      else if( _current_bid->get_price() > max_short_bid )
                         market_stat->avg_price_1h.ratio += max_short_bid.ratio;
                      else
                         market_stat->avg_price_1h.ratio += _current_bid->get_price().ratio;

                      if( _current_ask->get_price() < min_cover_ask )
                         market_stat->avg_price_1h.ratio += min_cover_ask.ratio;
                      else if( _current_ask->get_price() > max_short_bid )
                         market_stat->avg_price_1h.ratio += max_short_bid.ratio;
                      else
                         market_stat->avg_price_1h.ratio += _current_ask->get_price().ratio;
                   }

                   market_stat->avg_price_1h.ratio /= (BTS_BLOCKCHAIN_BLOCKS_PER_HOUR+1);
                }
             }

             if( pending_block_num >= BTSX_MARKET_FORK_5_BLOCK_NUM )
             {
                 if( quote_asset->is_market_issued() && base_id == 0 )
                 {
                     if( market_stat->ask_depth < 500000000000ll
                         || market_stat->bid_depth < 500000000000ll )
                     {
                        _market_transactions.clear(); // nothing should have executed
                       std::string reason = "After executing orders there was insufficient depth remaining";
                       FC_CAPTURE_AND_THROW( insufficient_depth, (reason)(market_stat) );
                     }
                 }
             }
             else
             {
                 if( quote_asset->is_market_issued() )
                 {
                     if( pending_block_num >= BTSX_MARKET_FORK_3_BLOCK_NUM )
                     {
                        if( market_stat->ask_depth < 1000000000000ll
                            || market_stat->bid_depth < 1000000000000ll )
                        {
                           _market_transactions.clear(); // nothing should have executed
                          std::string reason = "After executing orders there was insufficient depth remaining";
                          FC_CAPTURE_AND_THROW( insufficient_depth, (reason)(market_stat) );
                        }
                     }
                     else
                     {
                        if( market_stat->ask_depth < 2000000000000ll
                            || market_stat->bid_depth < 2000000000000ll )
                        {
                           _market_transactions.clear(); // nothing should have executed
                          std::string reason = "After executing orders there was insufficient depth remaining";
                          FC_CAPTURE_AND_THROW( insufficient_depth, (reason)(market_stat) );
                        }
                     }
                 }
             }
             _pending_state->store_market_status( *market_stat );

             update_market_history( trading_volume, opening_price, closing_price, market_stat, timestamp );

             wlog( "done matching orders" );
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
          FC_ASSERT( mtrx.bid_paid.amount >= 0 );
          FC_ASSERT( mtrx.ask_paid.amount >= 0 );
          FC_ASSERT( mtrx.bid_received.amount >= 0 );
          FC_ASSERT( mtrx.ask_received .amount>= 0 );
          FC_ASSERT( mtrx.bid_paid >= mtrx.ask_received );
          FC_ASSERT( mtrx.ask_paid >= mtrx.bid_received );
          FC_ASSERT( mtrx.fees_collected.amount >= 0 );

          //elog( "${trx}", ("trx", fc::json::to_pretty_string( mtrx ) ) );

          _market_transactions.push_back(mtrx);
      } FC_CAPTURE_AND_RETHROW( (mtrx) ) }

      void pay_current_short(const market_transaction& mtrx, const asset& xts_paid_by_short, asset_record& quote_asset  )
      { try {
          FC_ASSERT( _current_bid->type == short_order );
          FC_ASSERT( mtrx.bid_type == short_order );

          /** NOTE: the short may pay extra XTS to the collateral in the event rounding occurs.  This
           * just checks to make sure it is reasonable.
           */
          FC_ASSERT( mtrx.ask_paid <= xts_paid_by_short, "", ("mtrx",mtrx)("xts_paid_by_short",xts_paid_by_short) );
          /** sanity check to make sure that the only difference is rounding error */
          FC_ASSERT( mtrx.ask_paid.amount >= (xts_paid_by_short.amount - BTS_BLOCKCHAIN_PRECISION), "", ("mtrx",mtrx)("xts_paid_by_short",xts_paid_by_short) );

          quote_asset.current_share_supply += mtrx.bid_paid.amount;

          auto collateral  = xts_paid_by_short + mtrx.ask_paid;
          if( mtrx.bid_paid.amount <= 0 ) // WHY is this ever negitive??
          {
             FC_ASSERT( mtrx.bid_paid.amount >= 0 );
             //ulog( "bid paid ${c}  collateral ${xts} \nbid: ${current}\nask: ${ask}", ("c",mtrx.bid_paid)("xts",xts_paid_by_short)("current", (*_current_bid))("ask",*_current_ask) );
             _current_bid->state.balance -= xts_paid_by_short.amount;
             return;
          }

          auto cover_price = mtrx.bid_price;
          cover_price.ratio *= 3;
          cover_price.ratio /= 4;
         // auto cover_price = mtrx.bid_paid / asset( (3*collateral.amount)/4, _base_id );

          market_index_key cover_index( cover_price, _current_bid->get_owner() );
          auto ocover_record = _pending_state->get_collateral_record( cover_index );

          if( NOT ocover_record ) ocover_record = collateral_record();

          ocover_record->collateral_balance += collateral.amount;
          ocover_record->payoff_balance += mtrx.bid_paid.amount;

          FC_ASSERT( ocover_record->payoff_balance >= 0, "", ("record",ocover_record) );
          FC_ASSERT( ocover_record->collateral_balance >= 0 , "", ("record",ocover_record));

          _current_bid->state.balance -= xts_paid_by_short.amount;
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

                auto prev_accumulated_fees = _pending_state->get_accumulated_fees();
                _pending_state->set_accumulated_fees( prev_accumulated_fees + fee );

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

      void accumulate_fees( const market_transaction& mtrx, asset_record& quote_asset )
      {
         if( mtrx.fees_collected.amount == 0 ) return;
         if( mtrx.fees_collected.asset_id == 0 )
         {
             auto prev_accumulated_fees = _pending_state->get_accumulated_fees();
             _pending_state->set_accumulated_fees( prev_accumulated_fees + mtrx.fees_collected.amount );
         }
         else
         {
            FC_ASSERT( quote_asset.id == mtrx.fees_collected.asset_id );
            quote_asset.collected_fees += mtrx.fees_collected.amount;
         }
      }

      bool get_next_bid()
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
            //wlog( "SHORT ITER VALID: ${o}", ("o",bid) );
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
            // wlog( "           No Shorts         ****   " );
         }
         if( _bid_itr.valid() ) --_bid_itr;
         return _current_bid.valid();
      } FC_CAPTURE_AND_RETHROW() }

      bool get_next_ask()
      { try {
         if( _current_ask && _current_ask->state.balance > 0 )
         {
            //idump( (_current_ask) );
            return _current_ask.valid();
         }
         reset_current_ask();
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

            if( _pending_state->get_head_block_num() >= BTSX_MARKET_FORK_4_BLOCK_NUM )
               return _current_ask.valid();

            return true;
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
               assert(_pending_state->get_head_block_num() < BTSX_MARKET_FORK_4_BLOCK_NUM || _current_ask);
               if (_pending_state->get_head_block_num() >= BTSX_MARKET_FORK_4_BLOCK_NUM && !_current_ask)
                 FC_THROW_EXCEPTION(evaluation_error, "no current_ask"); // should never happen, but if it does, don't swap in the backup ask
               save_and_restore_ask current_ask_swapper(_current_ask, _current_ask_backup);

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
      void reset_current_ask()
      {
        if (_current_ask)
        {
          if (_pending_state->get_head_block_num() < BTSX_MARKET_FORK_4_BLOCK_NUM)
            _current_ask_backup = _current_ask;
          _current_ask.reset();
        }
         /* does not reset _current_ask_backup */
      }

      pending_chain_state_ptr     _pending_state;
      pending_chain_state_ptr     _prior_state;
      chain_database_impl&        _db_impl;

      optional<market_order>      _current_bid;
      optional<market_order>      _current_ask;
      optional<market_order>      _current_ask_backup; // used to allow us to validate blocks before BTSX_MARKET_FORK_4_BLOCK_NUM
      share_type                  _current_payoff_balance;
      asset_id_type               _quote_id;
      asset_id_type               _base_id;

      int                         _orders_filled;

      vector<market_transaction>  _market_transactions;

      bts::db::cached_level_map< market_index_key, order_record >::iterator       _bid_itr;
      bts::db::cached_level_map< market_index_key, order_record >::iterator       _ask_itr;
      bts::db::cached_level_map< market_index_key, order_record >::iterator       _short_itr;
      bts::db::cached_level_map< market_index_key, collateral_record >::iterator  _collateral_itr;
};
