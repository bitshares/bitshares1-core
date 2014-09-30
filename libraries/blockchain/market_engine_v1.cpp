#include <bts/blockchain/market_engine_v1.hpp>

namespace bts { namespace blockchain { namespace detail {

      market_engine_v1::market_engine_v1( pending_chain_state_ptr ps, chain_database_impl& cdi )
      :_pending_state(ps),_db_impl(cdi)
      {
          _pending_state = std::make_shared<pending_chain_state>( ps );
          _prior_state = ps;
      }

      bool market_engine_v1::execute( asset_id_type quote_id, asset_id_type base_id, const fc::time_point_sec& timestamp )
      {
         try {
             _quote_id = quote_id;
             _base_id = base_id;
             auto quote_asset = _pending_state->get_asset_record( _quote_id );

             // DISABLE MARKET ISSUED ASSETS
             if( quote_asset->is_market_issued() )
                return false; // don't execute anything.

             // the order book is soreted from low to high price, so to get the last item (highest bid), we need to go to the first item in the
             // next market class and then back up one
             auto next_pair  = base_id+1 == quote_id ? price( 0, quote_id+1, 0) : price( 0, quote_id, base_id+1 );
             _bid_itr        = _db_impl._bid_db.lower_bound( market_index_key( next_pair ) );
             _ask_itr        = _db_impl._ask_db.lower_bound( market_index_key( price( 0, quote_id, base_id) ) );
             _short_itr      = _db_impl._short_db.lower_bound( market_index_key( next_pair ) );
             _collateral_itr      = _db_impl._collateral_db.lower_bound( market_index_key( next_pair ) );

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

             asset consumed_bid_depth(0,base_id);
             asset consumed_ask_depth(0,base_id);


             asset usd_fees_collected(0,quote_id);
             asset trading_volume(0, base_id);

             omarket_status market_stat = _pending_state->get_market_status( _quote_id, _base_id );
             if( !market_stat.valid() )
             {
                if( quote_asset->is_market_issued() ) FC_CAPTURE_AND_THROW( insufficient_depth, (market_stat) );
                FC_ASSERT( market_stat.valid() );
             }

             while( get_next_bid() && get_next_ask() )
             {
                idump( (_current_bid)(_current_ask) );
                price ask_price = _current_ask->get_price();
                // this works for bids, asks, and shorts.... but in the case of a cover
                // the current ask can go lower than the call price in order to match
                // the bid....
                if( _current_ask->type == cover_order )
                {
                   ask_price = std::min( _current_bid->get_price(), _current_ask->get_highest_cover_price() );
                }

                if( _current_bid->get_price() < ask_price )
                   break;

                if( quote_asset->is_market_issued() )
                {
                   if( !market_stat ||
                       market_stat->ask_depth < 1000000000000ll ||
                       market_stat->bid_depth < 1000000000000ll
                     )
                     FC_CAPTURE_AND_THROW( insufficient_depth, (market_stat) );
                }

                auto quantity = std::min( _current_bid->get_quantity(), _current_ask->get_quantity() );

                auto usd_paid_by_bid     = quantity * _current_bid->get_price();
                auto usd_received_by_ask = quantity * _current_ask->get_price();
                auto xts_paid_by_ask     = quantity;
                auto xts_received_by_bid = quantity;

                consumed_bid_depth += quantity;
                consumed_ask_depth += quantity;

                if( _current_bid->type == short_order )
                {
                   usd_paid_by_bid = usd_received_by_ask;
                }

                if( _current_ask->type == cover_order )
                {
                    usd_received_by_ask = usd_paid_by_bid;
                }

                FC_ASSERT( usd_paid_by_bid.amount >= 0 );
                FC_ASSERT( xts_paid_by_ask.amount >= 0 );
                FC_ASSERT( usd_received_by_ask.amount >= 0 );
                FC_ASSERT( xts_received_by_bid.amount >= 0 );
                FC_ASSERT( usd_paid_by_bid >= usd_received_by_ask );
                FC_ASSERT( xts_paid_by_ask >= xts_received_by_bid );

                // sanity check to keep supply from growing without bound
                FC_ASSERT( usd_paid_by_bid < asset(quote_asset->maximum_share_supply,quote_id), "", ("usd_paid_by_bid",usd_paid_by_bid)("asset",quote_asset) );

                usd_fees_collected += usd_paid_by_bid - usd_received_by_ask;
                idump( (usd_fees_collected)(xts_paid_by_ask)(xts_received_by_bid)(quantity) );

                market_transaction mtrx;
                mtrx.bid_owner       = _current_bid->get_owner();
                mtrx.ask_owner       = _current_ask->get_owner();
                mtrx.bid_price       = _current_bid->get_price();
                mtrx.ask_price       = ask_price;
                mtrx.bid_paid        = usd_paid_by_bid;
                mtrx.bid_received    = xts_received_by_bid;
                mtrx.ask_paid        = xts_paid_by_ask;
                mtrx.ask_received    = usd_received_by_ask;
                mtrx.bid_type        = _current_bid->type;
                mtrx.fees_collected  = xts_paid_by_ask - xts_received_by_bid;

                _market_transactions.push_back(mtrx);
                trading_volume += mtrx.bid_received;

                market_stat->ask_depth -= xts_paid_by_ask.amount;
                if( _current_ask->type == ask_order )
                {
                   /* rounding errors on price cause this not to go to 0 in some cases */
                   if( quantity == _current_ask->get_quantity() )
                      _current_ask->state.balance = 0;
                   else
                      _current_ask->state.balance -= xts_paid_by_ask.amount;

                   FC_ASSERT( _current_ask->state.balance >= 0 );

                   auto ask_balance_address = withdraw_condition( withdraw_with_signature(_current_ask->get_owner()), quote_id ).get_address();
                   auto ask_payout = _pending_state->get_balance_record( ask_balance_address );
                   if( !ask_payout )
                      ask_payout = balance_record( _current_ask->get_owner(), asset(0,quote_id), 0 );
                   ask_payout->balance += usd_received_by_ask.amount;
                   ask_payout->last_update = _pending_state->now();
                   ask_payout->deposit_date = _pending_state->now();

                   _pending_state->store_balance_record( *ask_payout );
                   _pending_state->store_ask_record( _current_ask->market_index, _current_ask->state );
                }
                else if( _current_ask->type == cover_order )
                {
                   elog( "MATCHING COVER ORDER recv_usd: ${usd}  paid_collat: ${c}",
                         ("usd",usd_received_by_ask)("c",xts_paid_by_ask) );
                   wlog( "current ask: ${c}", ("c",_current_ask) );
                   // we are in the margin call range...
                   _current_ask->state.balance  -= usd_received_by_ask.amount;
                   *(_current_ask->collateral)  -= xts_paid_by_ask.amount;

                   FC_ASSERT( _current_ask->state.balance >= 0 );
                   FC_ASSERT( *_current_ask->collateral >= 0 );

                   if( _current_ask->state.balance == 0 ) // no more USD left
                   { // send collateral home to mommy & daddy
                         wlog( "            collateral balance is now 0!" );
                         auto ask_balance_address = withdraw_condition(
                                                           withdraw_with_signature(_current_ask->get_owner()),
                                                           base_id ).get_address();

                         auto ask_payout = _pending_state->get_balance_record( ask_balance_address );
                         if( !ask_payout )
                            ask_payout = balance_record( _current_ask->get_owner(), asset(0,base_id), 0 );
                         ask_payout->balance += (*_current_ask->collateral);
                         ask_payout->last_update = _pending_state->now();
                         ask_payout->deposit_date = _pending_state->now();

                         _pending_state->store_balance_record( *ask_payout );
                         _current_ask->collateral = 0;

                   }
                   wlog( "storing collateral ${c}", ("c",_current_ask) );
                   _pending_state->store_collateral_record( _current_ask->market_index,
                                                            collateral_record( *_current_ask->collateral,
                                                                               _current_ask->state.balance ) );
                }

                if( _current_bid->type == bid_order )
                {
                   _current_bid->state.balance -= usd_paid_by_bid.amount;
                   FC_ASSERT( _current_bid->state.balance >= 0 );

                   auto bid_payout = _pending_state->get_balance_record(
                                             withdraw_condition( withdraw_with_signature(_current_bid->get_owner()), base_id ).get_address() );
                   if( !bid_payout )
                      bid_payout = balance_record( _current_bid->get_owner(), asset(0,base_id), 0 );
                   bid_payout->balance += xts_received_by_bid.amount;
                   bid_payout->last_update = _pending_state->now();
                   bid_payout->deposit_date = _pending_state->now();
                   _pending_state->store_balance_record( *bid_payout );
                   _pending_state->store_bid_record( _current_bid->market_index, _current_bid->state );

                }
                else if( _current_bid->type == short_order )
                {
                   market_stat->bid_depth -= xts_received_by_bid.amount;

                   // TODO: what if the amount paid is 0 for bid and ask due to rounding errors,
                   // make sure this doesn't put us in an infinite loop.
                   if( quantity == _current_bid->get_quantity() )
                      _current_bid->state.balance = 0;
                   else
                      _current_bid->state.balance -= xts_received_by_bid.amount;
                   FC_ASSERT( _current_bid->state.balance >= 0 );

                   auto collateral = (xts_paid_by_ask + xts_received_by_bid).amount;
                   auto cover_price = usd_received_by_ask / asset( (3*collateral)/4, base_id );

                   market_index_key cover_index( cover_price, _current_ask->get_owner() );
                   auto ocover_record = _pending_state->get_collateral_record( cover_index );

                   if( NOT ocover_record )
                      ocover_record = collateral_record();

                   ocover_record->collateral_balance += collateral;
                   ocover_record->payoff_balance += usd_received_by_ask.amount;
                   FC_ASSERT( ocover_record->payoff_balance >= 0 );
                   FC_ASSERT( ocover_record->collateral_balance >= 0 );
                   _pending_state->store_collateral_record( cover_index, *ocover_record );

                   _pending_state->store_short_record( _current_bid->market_index, _current_bid->state );
                }
             } // while bid && ask

             if( quote_asset->is_market_issued() )
             {
                if( !market_stat ||
                    market_stat->ask_depth < 1000000000000ll ||
                    market_stat->bid_depth < 1000000000000ll
                  )
                  FC_CAPTURE_AND_THROW( insufficient_depth, (market_stat) );
             }
             _pending_state->store_market_status( *market_stat );

             if( trading_volume.amount > 0 && get_next_bid() && get_next_ask() )
             {
               market_history_key key(quote_id, base_id, market_history_key::each_block, _db_impl._head_block_header.timestamp);
               market_history_record new_record(_current_bid->get_price(), _current_ask->get_price(), price(), price(), trading_volume.amount);
               //LevelDB iterators are dumb and don't support proper past-the-end semantics.
               auto last_key_itr = _db_impl._market_history_db.lower_bound(key);
               if( !last_key_itr.valid() )
                 last_key_itr = _db_impl._market_history_db.last();
               else
                 --last_key_itr;

               key.timestamp = timestamp;

               //Unless the previous record for this market is the same as ours...
               if( (!(last_key_itr.valid()
                   && last_key_itr.key().quote_id == quote_id
                   && last_key_itr.key().base_id == base_id
                   && last_key_itr.key().granularity == market_history_key::each_block
                   && last_key_itr.value() == new_record)) )
               {
                 //...add a new entry to the history table.
                 _pending_state->market_history[key] = new_record;
               }

               fc::time_point_sec start_of_this_hour = timestamp - (timestamp.sec_since_epoch() % (60*60));
               market_history_key old_key(quote_id, base_id, market_history_key::each_hour, start_of_this_hour);
               if( auto opt = _db_impl._market_history_db.fetch_optional(old_key) )
               {
                 auto old_record = *opt;
                 old_record.volume += new_record.volume;
                 if( new_record.highest_bid > old_record.highest_bid || new_record.lowest_ask < old_record.lowest_ask )
                 {
                   old_record.highest_bid = std::max(new_record.highest_bid, old_record.highest_bid);
                   old_record.lowest_ask = std::min(new_record.lowest_ask, old_record.lowest_ask);
                   _pending_state->market_history[old_key] = old_record;
                 }
               }
               else
                 _pending_state->market_history[old_key] = new_record;

               fc::time_point_sec start_of_this_day = timestamp - (timestamp.sec_since_epoch() % (60*60*24));
               old_key = market_history_key(quote_id, base_id, market_history_key::each_day, start_of_this_day);
               if( auto opt = _db_impl._market_history_db.fetch_optional(old_key) )
               {
                 auto old_record = *opt;
                 old_record.volume += new_record.volume;
                 if( new_record.highest_bid > old_record.highest_bid || new_record.lowest_ask < old_record.lowest_ask )
                 {
                   old_record.highest_bid = std::max(new_record.highest_bid, old_record.highest_bid);
                   old_record.lowest_ask = std::min(new_record.lowest_ask, old_record.lowest_ask);
                   _pending_state->market_history[old_key] = old_record;
                 }
               }
               else
                 _pending_state->market_history[old_key] = new_record;
             }

             auto market_state = _pending_state->get_market_status( quote_id, base_id );
              if( !market_state )
                 market_state = market_status( quote_id, base_id, 0, 0 );
             market_state->last_error.reset();
             _pending_state->store_market_status( *market_state );

             wlog( "done matching orders" );
             _pending_state->apply_changes();
             return true;
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
        return false;
      } // execute(...)

      bool market_engine_v1::get_next_bid()
      { try {
         if( _current_bid && _current_bid->get_quantity().amount > 0 )
            return _current_bid.valid();

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
            wlog( "SHORT ITER VALID: ${o}", ("o",bid) );
            if( bid.get_price().quote_asset_id == _quote_id &&
                bid.get_price().base_asset_id == _base_id )
            {
                if( !_current_bid || _current_bid->get_price() < bid.get_price() )
                {
                   --_short_itr;
                   _current_bid = bid;
                   ilog( "returning ${v}", ("v",_current_bid.valid() ) );
                   return _current_bid.valid();
                }
            }
         }
         else
         {
            wlog( "           No Shorts         ****   " );
         }
         if( _bid_itr.valid() ) --_bid_itr;
         return _current_bid.valid();
      } FC_CAPTURE_AND_RETHROW() }

      bool market_engine_v1::get_next_ask()
      { try {
         if( _current_ask && _current_ask->state.balance > 0 )
         {
            wlog( "current ask" );
            return _current_ask.valid();
         }
         _current_ask.reset();

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
                   FC_CAPTURE_AND_THROW( insufficient_collateral, (_current_bid)(cover_ask)(cover_ask.get_highest_cover_price()));
                   --_collateral_itr;
                   continue;
                }
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
            wlog( "ASK ITER VALID: ${o}", ("o",ask) );
            if( ask.get_price().quote_asset_id == _quote_id &&
                ask.get_price().base_asset_id == _base_id )
            {
                _current_ask = ask;
            }
            ++_ask_itr;
            return true;
         }
         return _current_ask.valid();
      } FC_CAPTURE_AND_RETHROW() }

} } } // end namespace bts::blockchain::detail
