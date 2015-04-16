#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/market_engine.hpp>
#include <fc/real128.hpp>
#include <algorithm>

namespace bts { namespace blockchain { namespace detail {

#define MARKET_ENGINE_PASS_PROCESS_MARGIN_CALLS   0
#define MARKET_ENGINE_PASS_PROCESS_EXPIRED_COVERS 1
#define MARKET_ENGINE_PASS_PROCESS_ASK_ORDERS     2
#define MARKET_ENGINE_PASS_COUNT                  3

market_engine::market_engine( const pending_chain_state_ptr ps, const chain_database_impl& cdi )
: _pending_state( ps ), _db_impl( cdi )
{
    _pending_state = std::make_shared<pending_chain_state>( ps );
    _prior_state = ps;
}

bool market_engine::execute( const asset_id_type quote_id, const asset_id_type base_id, const time_point_sec timestamp )
{ try {
    try
    {
        _quote_id = quote_id;
        _base_id = base_id;

        oasset_record quote_asset = _pending_state->get_asset_record( _quote_id );
        oasset_record base_asset = _pending_state->get_asset_record( _base_id );

        FC_ASSERT( quote_asset.valid() && base_asset.valid() );
        FC_ASSERT( !quote_asset->flag_is_active( asset_record::halted_markets ) );
        FC_ASSERT( !base_asset->flag_is_active( asset_record::halted_markets ) );

        FC_ASSERT( !_pending_state->is_fraudulent_asset( *quote_asset ) );
        FC_ASSERT( !_pending_state->is_fraudulent_asset( *base_asset ) );

        // The order book is sorted from low to high price. So to get the last item (highest bid),
        // we need to go to the first item in the next market class and then back up one
        const price current_pair = price( 0, quote_id, base_id );
        const price next_pair = (base_id+1 == quote_id) ? price( 0, quote_id+1, 0 ) : price( 0, quote_id, base_id+1 );

        _bid_itr = _db_impl._bid_db.lower_bound( market_index_key( next_pair ) );
        _ask_itr = _db_impl._ask_db.lower_bound( market_index_key( price( 0, quote_id, base_id) ) );
        _collateral_itr = _db_impl._collateral_db.lower_bound( market_index_key( next_pair ) );

        int last_orders_filled = -1;
        asset base_volume( 0, base_id );
        asset quote_volume( 0, quote_id );
        price opening_price, closing_price, highest_price, lowest_price;

        if( !_ask_itr.valid() ) _ask_itr = _db_impl._ask_db.begin();

        //
        // following logic depends on the internals of cached_level_map class.
        // if lower_bound() argument is past the end of the collection,
        // then result will point to end() which has valid() == false
        //
        // so we need to decrement the returned iterator, but
        // decrement operator may be undefined for end(), so we
        // special-case the result.
        //
        // one day we should write a greatest_element_below() method
        // for our DB class to implement the logic we're doing
        // directly here
        //
        // decrementing begin() OTOH is well-defined and returns
        // end() which is invalid, thus overflow of --itr here is
        // ok as long as we check valid() later
        //
        if( _bid_itr.valid() ) --_bid_itr;
        else _bid_itr = _db_impl._bid_db.last();

        if( _collateral_itr.valid() ) --_collateral_itr;
        else _collateral_itr = _db_impl._collateral_db.last();

        // Market issued assets cannot match until the first time there is a median feed; assume feed price base id 0
        if( quote_asset->is_market_issued() && base_asset->id == 0 )
        {
            _feed_price = _db_impl.self->get_active_feed_price( _quote_id );
            const ostatus_record market_stat = _pending_state->get_status_record( status_index{ _quote_id, _base_id } );
            if( (!market_stat.valid() || !market_stat->last_valid_feed_price.valid()) && !_feed_price.valid() )
                FC_CAPTURE_AND_THROW( insufficient_feeds, (quote_id)(base_id) );

            if( _feed_price.valid() )
            {
                auto short_iter = _db_impl._short_db.lower_bound( market_index_key( next_pair ) );
                if( short_iter.valid() ) --short_iter;
                else short_iter = _db_impl._short_db.last();

                for( ; short_iter.valid(); --short_iter )
                {
                    const market_index_key& key = short_iter.key();
                    if( key.order_price.quote_asset_id != _quote_id || key.order_price.base_asset_id != _base_id )
                        break;

                    const order_record& order = short_iter.value();
                    if( !order.limit_price.valid() )
                    {
                        _stuck_shorts[ key ] = order;
                    }
                    else
                    {
                        if( *order.limit_price >= *_feed_price)
                            _stuck_shorts[ key ] = order;
                        else
                            _unstuck_shorts[ std::make_pair( *order.limit_price, key ) ] = order;
                    }
                }
            }
        }

        _stuck_shorts_iter = _stuck_shorts.rbegin();
        _unstuck_shorts_iter = _unstuck_shorts.rbegin();

        const expiration_index exp_index{ quote_id, time_point(), market_index_key( current_pair ) };
        _collateral_expiration_itr = _db_impl._collateral_expiration_index.lower_bound( exp_index );

        _current_bid.reset();
        for( _current_pass = 0; _current_pass < MARKET_ENGINE_PASS_COUNT; _current_pass++ )
        {
            _current_ask.reset();
            while( true )
            {
                if( (!_current_bid.valid()) || (_current_bid->get_balance().amount <= 0) )
                {
                    ilog("getting next bid");
                    get_next_bid();
                    if( (!_current_bid.valid()) )
                    {
                        ilog("market engine terminating due to no more bids");
                        break;
                    }
                    idump( (_current_bid) );
                }
                if( (!_current_ask.valid()) || (_current_ask->get_balance().amount <= 0) )
                {
                    ilog("getting next ask");
                    get_next_ask();
                    if( (!_current_ask.valid()) )
                    {
                        ilog("market engine terminating due to no more asks");
                        break;
                    }
                    idump( (_current_ask) );
                }

                // Make sure that at least one order was matched every time we enter the loop
                FC_ASSERT( _orders_filled != last_orders_filled, "We appear caught in an order matching loop!" );
                last_orders_filled = _orders_filled;

                // Initialize the market transaction
                market_transaction mtrx;
                mtrx.bid_index.owner = _current_bid->get_owner();
                mtrx.ask_index.owner = _current_ask->get_owner();
                if( _feed_price )
                {
                   mtrx.bid_index.order_price = _current_bid->get_price( *_feed_price );
                   mtrx.ask_index.order_price = _current_ask->get_price( *_feed_price );
                }
                else
                {
                   mtrx.bid_index.order_price = _current_bid->get_price();
                   mtrx.ask_index.order_price = _current_ask->get_price();
                }
                mtrx.bid_type  = _current_bid->type;
                mtrx.ask_type  = _current_ask->type;

                if( _current_bid->type == short_order )
                {
                    FC_ASSERT( quote_asset->is_market_issued() );
                    // get_next_bid() shouldn't return shorts if there's no feed
                    FC_ASSERT( _feed_price.valid() );

                    // Skip shorts that are over the price limit
                    if( _current_bid->state.limit_price.valid() )
                    {
                      mtrx.bid_index.order_price = std::min( *_current_bid->state.limit_price, *_feed_price );
                    }
                    else
                    {
                      mtrx.bid_index.order_price = *_feed_price;
                    }
                }

                if( _current_ask->type == cover_order )
                {
                    FC_ASSERT( quote_asset->is_market_issued() );
                    // get_next_ask() shouldn't return covers if there's no feed
                    FC_ASSERT( _feed_price.valid() );

                    /**
                    *  Don't allow margin calls to be executed too far below
                    *  the minimum ask, this could lead to an attack where someone
                    *  walks the whole book to steal the collateral.
                    */
                    if( mtrx.bid_index.order_price < minimum_cover_ask_price() )
                    {
                        ilog("terminating cover match iteration because bid doesn't meet minimum_cover_ask_price()");
                        break;
                    }

                    if( mtrx.ask_index.order_price > *_feed_price )
                    {
                        // margin called cover
                        mtrx.ask_index.order_price = mtrx.bid_index.order_price;
                        ilog("cover is margin call executing at price ${p}", ("p",mtrx.ask_index.order_price));
                    }
                    else if( (*_current_ask->expiration) <= _pending_state->now() )
                    {
                        // expired cover which has not been margin called
                        mtrx.ask_index.order_price = *_feed_price;
                        ilog("cover is executing at feed price ${p}", ("p",mtrx.ask_index.order_price));
                    }
                    else
                    {
                        // inactive cover
                        FC_ASSERT(false, "get_next_ask() returned inactive cover");
                    }
                }

                //
                // get_next_bid() always returns the best bid
                // get_next_ask() always returns the next ask
                //
                // by "best" I mean that no other order can match (in the current pass) if the best order does not match
                // so we can terminate the current pass as soon as the best orders don't match each other
                //
                if( mtrx.bid_index.order_price < mtrx.ask_index.order_price )
                {
                   wlog( "bid_price ${b} < ask_price ${a}; exit market loop", ("b",mtrx.bid_index.order_price)("a",mtrx.ask_index.order_price) );
                   break;
                }

                if( _current_ask->type == cover_order && _current_bid->type == short_order )
                {
                    price collateral_rate                = mtrx.bid_index.order_price;
                    collateral_rate.ratio                /= 2; // 2x from short, 1 x from long == 3x default collateral
                    const asset cover_collateral         = asset( *_current_ask->collateral, _base_id );
                    const asset max_usd_cover_can_afford = cover_collateral * mtrx.bid_index.order_price;
                    const asset cover_debt               = get_current_cover_debt();
                    const asset usd_for_short_sale       = _current_bid->get_balance() * collateral_rate;

                    const price liquidation_price = cover_debt / cover_collateral;

                    if( liquidation_price > *_feed_price )
                    {
                        handle_liquidation( liquidation_price );
                        break;
                    }

                    //Actual quote to purchase is the minimum of what's for sale, what can I possibly buy, and what I owe
                    const asset usd_exchanged = std::min( {usd_for_short_sale, max_usd_cover_can_afford, cover_debt} );

                    mtrx.ask_received   = usd_exchanged;

                    /** handle rounding errors */
                    // if cover collateral was completely consumed without paying off all USD
                    if( usd_exchanged == max_usd_cover_can_afford )
                       mtrx.ask_paid = cover_collateral;
                    else  // the short was completely consumed
                       mtrx.ask_paid = mtrx.ask_received * mtrx.ask_index.order_price;

                    mtrx.bid_received = mtrx.ask_paid;
                    mtrx.bid_paid = mtrx.ask_received;

                    /** handle rounding errors */
                    if( usd_exchanged == usd_for_short_sale ) // filled full short, consume all collateral
                       mtrx.short_collateral = _current_bid->get_balance();
                    else
                       mtrx.short_collateral = mtrx.bid_paid * collateral_rate; /** note rounding errors handled in pay_current_short */

                    pay_current_short( mtrx, *quote_asset, *base_asset );
                    pay_current_cover( mtrx, *quote_asset );
                }
                else if( _current_bid->type == bid_order && _current_ask->type == cover_order )
                {
                    const asset cover_collateral          = asset( *_current_ask->collateral, _base_id );
                    const asset max_usd_cover_can_afford  = cover_collateral * mtrx.bid_index.order_price;
                    const asset cover_debt                = get_current_cover_debt();
                    const asset usd_for_sale              = _current_bid->get_balance();

                    const price liquidation_price = cover_debt / cover_collateral;

                    if( liquidation_price > *_feed_price )
                    {
                        handle_liquidation( liquidation_price );
                        break;
                    }

                    const asset usd_exchanged = std::min( {usd_for_sale, max_usd_cover_can_afford, cover_debt} );

                    mtrx.ask_received = usd_exchanged;

                    /** handle rounding errors */
                    // if cover collateral was completely consumed without paying off all USD
                    if( mtrx.ask_received == max_usd_cover_can_afford )
                       mtrx.ask_paid = cover_collateral;
                    else // the bid was completely consumed
                       mtrx.ask_paid = mtrx.ask_received * mtrx.ask_index.order_price;

                    mtrx.bid_received = mtrx.ask_paid;
                    mtrx.bid_paid = mtrx.ask_received;

                    pay_current_bid( mtrx, *quote_asset, *base_asset );
                    pay_current_cover( mtrx, *quote_asset );
                }
                else if( _current_ask->type == ask_order && _current_bid->type == short_order )
                {
                    FC_ASSERT( _feed_price.valid() );

                    // Bound collateral ratio (maximizes collateral of new margin position)
                    price collateral_rate          = mtrx.bid_index.order_price;
                    collateral_rate.ratio          /= 2; // 2x from short, 1 x from long == 3x default collateral
                    const asset ask_quantity_usd   = _current_ask->get_quote_quantity(*_feed_price);
                    const asset short_quantity_usd = _current_bid->get_balance() * collateral_rate;
                    const asset usd_exchanged      = std::min( short_quantity_usd, ask_quantity_usd );

                    mtrx.ask_received = usd_exchanged;

                    /** handle rounding errors */
                    if( usd_exchanged == ask_quantity_usd )
                        mtrx.ask_paid = _current_ask->get_balance();
                    else
                        mtrx.ask_paid = mtrx.ask_received * mtrx.ask_index.order_price;

                    if( usd_exchanged == short_quantity_usd )
                        mtrx.short_collateral = _current_bid->get_balance();
                    else
                        mtrx.short_collateral = usd_exchanged * collateral_rate;

                    mtrx.bid_received = mtrx.ask_paid;
                    mtrx.bid_paid = mtrx.ask_received;

                    ilog("mtrx going into pay_current_short, pay_current_ask");
                    idump( (mtrx) );

                    pay_current_short( mtrx, *quote_asset, *base_asset );
                    pay_current_ask( mtrx, *quote_asset, *base_asset );
                }
                else if( _current_ask->type == ask_order && _current_bid->type == bid_order )
                {
                    const asset bid_quantity_xts = _current_bid->get_quantity( _feed_price ? *_feed_price : price() );
                    const asset ask_quantity_xts = _current_ask->get_quantity( _feed_price ? *_feed_price : price() );
                    const asset quantity_xts = std::min( bid_quantity_xts, ask_quantity_xts );

                    // Everyone gets the price they asked for
                    mtrx.ask_received   = quantity_xts * mtrx.ask_index.order_price;
                    mtrx.bid_paid       = quantity_xts * mtrx.bid_index.order_price;

                    mtrx.ask_paid       = quantity_xts;
                    mtrx.bid_received   = quantity_xts;

                    // Handle rounding errors
                    if( quantity_xts == bid_quantity_xts )
                       mtrx.bid_paid = _current_bid->get_balance();

                    if( quantity_xts == ask_quantity_xts )
                       mtrx.ask_paid = _current_ask->get_balance();

                    mtrx.quote_fees = mtrx.bid_paid - mtrx.ask_received;

                    pay_current_bid( mtrx, *quote_asset, *base_asset );
                    pay_current_ask( mtrx, *quote_asset, *base_asset );
                }

                push_market_transaction( mtrx );

                quote_asset->collected_fees += mtrx.quote_fees.amount;
                base_asset->collected_fees += mtrx.base_fees.amount;

                // base_id < quote_id,
                // ask is base -> quote (give out base and receive quote),
                // bid is quote -> base (give out quote and receive base)
                base_volume += mtrx.bid_received;
                quote_volume += mtrx.ask_received;
                if( opening_price == price() )
                  opening_price = mtrx.bid_index.order_price;
                closing_price = mtrx.bid_index.order_price;
                //
                // Remark: only prices of matched orders are used to update market history
                //
                // Because of prioritization, we need the second comparison
                // in the following if statements.  Ask-side orders are
                // only sorted by price within a single pass.
                //
                if( highest_price == price() || highest_price < mtrx.bid_index.order_price)
                  highest_price = mtrx.bid_index.order_price;
                // TODO check here: store lowest ask price or lowest bid price?
                if( lowest_price == price() || lowest_price > mtrx.ask_index.order_price)
                  lowest_price = mtrx.ask_index.order_price;
            }
        }

        // update any fees collected
        _pending_state->store_asset_record( *quote_asset );
        _pending_state->store_asset_record( *base_asset );

        // Update market status and market history
        {
            ostatus_record market_stat = _pending_state->get_status_record( status_index{ _quote_id, _base_id } );
            if( !market_stat.valid() ) market_stat = status_record( _quote_id, _base_id );
            market_stat->update_feed_price( _feed_price );
            market_stat->last_error.reset();
            _pending_state->store_status_record( *market_stat );

            // Remark: only prices of matched orders be updated to market history
            update_market_history( base_volume, quote_volume, highest_price, lowest_price,
                                   opening_price, closing_price, timestamp );
        }

        _pending_state->apply_changes();
        return true;
  }
  catch( const fc::exception& e )
  {
      wlog( "error executing market ${quote} / ${base}\n ${e}", ("quote",quote_id)("base",base_id)("e",e.to_detail_string()) );
      ostatus_record market_stat = _pending_state->get_status_record( status_index{ _quote_id, _base_id } );
      if( !market_stat.valid() ) market_stat = status_record( _quote_id, _base_id );
      market_stat->update_feed_price( _feed_price );
      market_stat->last_error = e;
      _prior_state->store_status_record( *market_stat );
  }
  return false;
} FC_CAPTURE_AND_RETHROW( (quote_id)(base_id)(timestamp) ) } // execute(...)

void market_engine::push_market_transaction( const market_transaction& mtrx )
{ try {
    // If not an automatic market cancel
    if( mtrx.ask_paid.amount != 0
        || mtrx.ask_received.amount != 0
        || mtrx.bid_received.asset_id != 0
        || mtrx.bid_paid.amount != 0 )
    {
        FC_ASSERT( mtrx.bid_paid.amount >= 0 );
        FC_ASSERT( mtrx.ask_paid.amount >= 0 );
        FC_ASSERT( mtrx.bid_received.amount >= 0 );
        FC_ASSERT( mtrx.ask_received.amount>= 0 );
        FC_ASSERT( mtrx.bid_paid >= mtrx.ask_received );
        FC_ASSERT( mtrx.ask_paid >= mtrx.bid_received );
        FC_ASSERT( mtrx.quote_fees.amount >= 0 );
        FC_ASSERT( mtrx.base_fees.amount >= 0 );
    }

    _market_transactions.push_back( mtrx );
} FC_CAPTURE_AND_RETHROW( (mtrx) ) }

void market_engine::pay_current_short( market_transaction& mtrx, asset_record& quote_asset, asset_record& base_asset )
{ try {
    FC_ASSERT( _current_bid->type == short_order );
    FC_ASSERT( mtrx.bid_type == short_order );

    // Because different collateral amounts create different orders, this prevents cover orders that
    // are too small to bother covering.
    if( (_current_bid->get_balance() - *mtrx.short_collateral).amount < base_asset.precision/100 )
    {
        if( _current_bid->get_balance() > *mtrx.short_collateral )
           *mtrx.short_collateral  += (_current_bid->get_balance() - *mtrx.short_collateral);
    }

    quote_asset.current_supply += mtrx.bid_paid.amount;

    auto collateral  = *mtrx.short_collateral + mtrx.ask_paid;
    if( mtrx.bid_paid.amount <= 0 )
    {
        FC_ASSERT( mtrx.bid_paid.amount >= 0 );
        _current_bid->state.balance -= mtrx.short_collateral->amount;
        return;
    }

    auto call_collateral = collateral;
    call_collateral.amount *= BTS_BLOCKCHAIN_MCALL_D2C_NUMERATOR;
    call_collateral.amount /= BTS_BLOCKCHAIN_MCALL_D2C_DENOMINATOR;
    auto cover_price = mtrx.bid_paid / call_collateral;

    market_index_key cover_index( cover_price, _current_bid->get_owner() );
    auto ocover_record = _pending_state->get_collateral_record( cover_index );

    if( NOT ocover_record ) ocover_record = collateral_record();

    ocover_record->collateral_balance += collateral.amount;
    ocover_record->payoff_balance += mtrx.bid_paid.amount;
    ocover_record->interest_rate = _current_bid->market_index.order_price;
    ocover_record->expiration = _pending_state->now() + BTS_BLOCKCHAIN_MAX_SHORT_PERIOD_SEC;

    FC_ASSERT( ocover_record->payoff_balance >= 0, "", ("record",ocover_record) );
    FC_ASSERT( ocover_record->collateral_balance >= 0 , "", ("record",ocover_record));
    FC_ASSERT( ocover_record->interest_rate.quote_asset_id > ocover_record->interest_rate.base_asset_id,
               "", ("record",ocover_record));

    _current_bid->state.balance -= mtrx.short_collateral->amount;

    FC_ASSERT( _current_bid->state.balance >= 0 );

    _pending_state->store_collateral_record( cover_index, *ocover_record );

    _pending_state->store_short_record( _current_bid->market_index, _current_bid->state );
} FC_CAPTURE_AND_RETHROW( (mtrx)(quote_asset)(base_asset) ) }

void market_engine::pay_current_bid( market_transaction& mtrx, asset_record& quote_asset, asset_record& base_asset )
{ try {
    FC_ASSERT( _current_bid->type == bid_order );
    FC_ASSERT( mtrx.bid_type == bid_order );

    _current_bid->state.balance -= mtrx.bid_paid.amount;
    FC_ASSERT( _current_bid->state.balance >= 0 );

    FC_ASSERT( base_asset.address_is_approved( *_pending_state, mtrx.bid_index.owner ) );

    auto bid_payout = _pending_state->get_balance_record(
                              withdraw_condition( withdraw_with_signature(mtrx.bid_index.owner), _base_id ).get_address() );
    if( !bid_payout )
        bid_payout = balance_record( mtrx.bid_index.owner, asset(0,_base_id), 0 );

    share_type issuer_fee = 0;
    if( base_asset.market_fee_rate > 0 )
    {
        FC_ASSERT( base_asset.market_fee_rate <= BTS_BLOCKCHAIN_MAX_UIA_MARKET_FEE_RATE );
        issuer_fee = mtrx.bid_received.amount * base_asset.market_fee_rate;
        issuer_fee /= BTS_BLOCKCHAIN_MAX_UIA_MARKET_FEE_RATE;
    }

    mtrx.base_fees.amount += issuer_fee;
    mtrx.bid_received.amount -= issuer_fee;

    bid_payout->balance += mtrx.bid_received.amount;
    bid_payout->last_update = _pending_state->now();
    bid_payout->deposit_date = _pending_state->now();
    _pending_state->store_balance_record( *bid_payout );

    // if the balance is less than 1 XTS then it gets collected as fees.
    if( (_current_bid->get_quote_quantity() * mtrx.bid_index.order_price).amount == 0 )
    {
        mtrx.quote_fees.amount += _current_bid->state.balance;
        _current_bid->state.balance = 0;
    }
    _pending_state->store_bid_record( _current_bid->market_index, _current_bid->state );
} FC_CAPTURE_AND_RETHROW( (mtrx)(quote_asset)(base_asset) ) }

void market_engine::pay_current_cover( market_transaction& mtrx, asset_record& quote_asset )
{ try {
    FC_ASSERT( _current_ask->type == cover_order );
    FC_ASSERT( mtrx.ask_type == cover_order );
    FC_ASSERT( _current_ask->interest_rate.valid() );
    FC_ASSERT( _current_ask->interest_rate->quote_asset_id > _current_ask->interest_rate->base_asset_id );

    const asset principle = _current_ask->get_balance();
    const auto cover_age = get_current_cover_age();
    const asset total_debt = get_current_cover_debt();

    asset principle_paid;
    asset interest_paid;
    if( mtrx.ask_received >= total_debt )
    {
        // Payoff the whole debt
        principle_paid = principle;
        interest_paid = mtrx.ask_received - principle_paid;
        _current_ask->state.balance = 0;
    }
    else
    {
        // Partial cover
        interest_paid = get_interest_paid( mtrx.ask_received, *_current_ask->interest_rate, cover_age );
        principle_paid = mtrx.ask_received - interest_paid;
        _current_ask->state.balance -= principle_paid.amount;
    }
    FC_ASSERT( principle_paid.amount >= 0 );
    FC_ASSERT( interest_paid.amount >= 0 );
    FC_ASSERT( _current_ask->state.balance >= 0 );

    *(_current_ask->collateral) -= mtrx.ask_paid.amount;

    FC_ASSERT( *_current_ask->collateral >= 0, "",
               ("mtrx",mtrx)("_current_ask", _current_ask)("interest_paid",interest_paid)  );

    quote_asset.current_supply -= principle_paid.amount;
    mtrx.quote_fees.amount += interest_paid.amount;

    // TODO: WTF is this?
    if( *_current_ask->collateral == 0 )
    {
        quote_asset.current_supply -= _current_ask->state.balance;
        _current_ask->state.balance = 0;
    }

    // If debt is fully paid off and there is leftover collateral
    if( _current_ask->state.balance == 0 && *_current_ask->collateral > 0 )
    { // send collateral home to mommy & daddy
          auto ask_balance_address = withdraw_condition(
                                            withdraw_with_signature(_current_ask->get_owner()),
                                            _base_id ).get_address();

          auto ask_payout = _pending_state->get_balance_record( ask_balance_address );
          if( !ask_payout )
              ask_payout = balance_record( _current_ask->get_owner(), asset(0,_base_id), 0 );

          auto left_over_collateral = (*_current_ask->collateral);

          ask_payout->balance += left_over_collateral;
          ask_payout->last_update = _pending_state->now();
          ask_payout->deposit_date = _pending_state->now();

          mtrx.returned_collateral = left_over_collateral;

          _pending_state->store_balance_record( *ask_payout );
          _current_ask->collateral = 0;
    }

    //
    // we need to translate _current_ask back into a collat_record in order to store it in the database
    //
    // this comment shows the various constructors in order to make it easier to verify that every field is written back correctly
    //
    //  market_order( order_type_enum t, market_index_key k, order_record s, share_type c, price interest, time_point_sec exp )
    //  :type(t),market_index(k),state(s),collateral(c),interest_rate(interest),expiration(exp){}
    //
    // market_order morder(
    //    cover_order,                                       // morder.type
    //    k,                                                 // morder.market_index
    //    order_record(collat_record->payoff_balance),       // morder.state
    //    collat_record->collateral_balance,                 // morder.collateral
    //    collat_record->interest_rate,                      // morder.interest_rate
    //    collat_record->expiration                          // morder.expiration
    //    );
    //
    //       collateral_record(share_type c = 0,
    //                     share_type p = 0,
    //                     const price& apr = price(),
    //                     time_point_sec exp = time_point_sec())
    //   :collateral_balance(c),payoff_balance(p),interest_rate(apr),expiration(exp){}
    //

    collateral_record collat_record(
        *_current_ask->collateral,               // originally initialized from collateral_balance
        _current_ask->get_balance().amount,      // get_balance() returns payoff_balance
        *_current_ask->interest_rate,
        *_current_ask->expiration
        );

    _pending_state->store_collateral_record( _current_ask->market_index,
                                             collat_record );
} FC_CAPTURE_AND_RETHROW( (mtrx)(quote_asset) ) }

void market_engine::pay_current_ask( market_transaction& mtrx, asset_record& quote_asset, asset_record& base_asset )
{ try {
    FC_ASSERT( _current_ask->type == ask_order );
    FC_ASSERT( mtrx.ask_type == ask_order );

    _current_ask->state.balance -= mtrx.ask_paid.amount;
    FC_ASSERT( _current_ask->state.balance >= 0, "balance: ${b}", ("b",_current_ask->state.balance) );

    FC_ASSERT( quote_asset.address_is_approved( *_pending_state, mtrx.ask_index.owner ) );

    auto ask_balance_address = withdraw_condition( withdraw_with_signature(mtrx.ask_index.owner), _quote_id ).get_address();
    auto ask_payout = _pending_state->get_balance_record( ask_balance_address );
    if( !ask_payout )
        ask_payout = balance_record( mtrx.ask_index.owner, asset(0,_quote_id), 0 );

    share_type issuer_fee = 0;
    if( quote_asset.market_fee_rate > 0 )
    {
        FC_ASSERT( quote_asset.market_fee_rate <= BTS_BLOCKCHAIN_MAX_UIA_MARKET_FEE_RATE );
        issuer_fee = mtrx.ask_received.amount * quote_asset.market_fee_rate;
        issuer_fee /= BTS_BLOCKCHAIN_MAX_UIA_MARKET_FEE_RATE;
    }

    mtrx.quote_fees.amount += issuer_fee;
    mtrx.ask_received.amount -= issuer_fee;

    ask_payout->balance += mtrx.ask_received.amount;
    ask_payout->last_update = _pending_state->now();
    ask_payout->deposit_date = _pending_state->now();

    _pending_state->store_balance_record( *ask_payout );

    // if the balance is less than 1 XTS * PRICE < .001 USD XTS goes to fees
    if( (_current_ask->get_quantity() * mtrx.ask_index.order_price).amount == 0 )
    {
        mtrx.base_fees.amount += _current_ask->state.balance;
        _current_ask->state.balance = 0;
    }

    _pending_state->store_ask_record( _current_ask->market_index, _current_ask->state );

} FC_CAPTURE_AND_RETHROW( (mtrx)(quote_asset)(base_asset) ) } // pay_current_ask

/**
 *  if there are bids above feed price, take the max of the two
 *  if there are shorts at the feed take the next short
 *  if there are bids with prices above the limit of the current short they should take priority
 *       - shorts need to be ordered by limit first, then interest rate *WHEN* the limit is
 *         lower than the feed price.
 */
bool market_engine::get_next_bid()
{ try {
    if( _current_bid && _current_bid->get_quantity().amount > 0 )
      return _current_bid.valid();

    ++_orders_filled;
    _current_bid.reset();

    // if we have no bids, the short wins as long as there is one,
    // so just call get_next_short() and be done
    if( !_bid_itr.valid() )
        return get_next_short();

    const market_order bid( bid_order, _bid_itr.key(), _bid_itr.value() );

    // if we iterated out of the market, there are no bids and the
    // short wins (as long as there is one), so handle this case
    // just like above
    if( bid.get_price().quote_asset_id != _quote_id || bid.get_price().base_asset_id != _base_id )
        return get_next_short();

    //
    // bid from itr is valid and in the correct market.
    // in order for short to win, _feed_price must be valid,
    // bid_price must be less than the feed.  then we call
    // to get_next_short() to actually fetch the short and
    // compare its price.
    //
    if( _feed_price.valid() && bid.get_price() < *_feed_price && get_next_short( bid ) )
        return true;

    _current_bid = bid;
    --_bid_itr;
    return true;
} FC_CAPTURE_AND_RETHROW() }

bool market_engine::get_next_short( const omarket_order& bid_being_considered )
{ try {
    // first consider shorts at the feed price
    if( _stuck_shorts_iter != _stuck_shorts.rend() )
    {
        const market_index_key& key = _stuck_shorts_iter->first;
        const order_record& order = _stuck_shorts_iter->second;
        _current_bid = market_order( short_order, key, order, order.balance, key.order_price );
        ++_stuck_shorts_iter;
        return true;
    }
    // then check shorts with a limit below the feed
    else if( _unstuck_shorts_iter != _unstuck_shorts.rend() )
    {
        FC_ASSERT( _feed_price.valid() );

        const price& limit_price = _unstuck_shorts_iter->first.first;
        const market_index_key& key = _unstuck_shorts_iter->first.second;
        const order_record& order = _unstuck_shorts_iter->second;

        // if the limit price is better than a current bid
        if( !bid_being_considered.valid() || limit_price > bid_being_considered->get_price( *_feed_price ) )
        {
            _current_bid = market_order( short_order, key, order, order.balance, key.order_price );
            ++_unstuck_shorts_iter;
            return true;
        }
    }

    return false;
} FC_CAPTURE_AND_RETHROW( (bid_being_considered) ) }

bool market_engine::get_next_ask()
{ try {
    if( _current_ask && _current_ask->state.balance > 0 )
      return _current_ask.valid();

    _current_ask.reset();
    ++_orders_filled;

    switch( _current_pass )
    {
    case MARKET_ENGINE_PASS_PROCESS_MARGIN_CALLS:
        return get_next_ask_margin_call();
    case MARKET_ENGINE_PASS_PROCESS_EXPIRED_COVERS:
        return get_next_ask_expired_cover();
    case MARKET_ENGINE_PASS_PROCESS_ASK_ORDERS:
        return get_next_ask_order();
    default:
        FC_ASSERT( false, "_current_pass value is unknown" );
    }
    // unreachable, but necessary to silence gcc compiler warning
    return false;
} FC_CAPTURE_AND_RETHROW() }

market_order market_engine::build_collateral_market_order( market_index_key k ) const
{
    // fetch collateral data for given market_index_key from PCS
    // and put it in a market_order
    ocollateral_record collat_record = _pending_state->get_collateral_record( k );

    FC_ASSERT( collat_record.valid() );

    market_order morder(
        cover_order,
        k,
        order_record(collat_record->payoff_balance),
        collat_record->collateral_balance,
        collat_record->interest_rate,
        collat_record->expiration
        );
    return morder;
}

bool market_engine::get_next_ask_margin_call()
{ try {
    /**
    *  Margin calls take priority over all other ask orders
    */
    while( _current_bid && _collateral_itr.valid() )
    {
      const market_order cover_ask = build_collateral_market_order( _collateral_itr.key() );

      if( cover_ask.get_price().quote_asset_id == _quote_id &&
          cover_ask.get_price().base_asset_id == _base_id )
      {
          // Don't cover unless the price is below the feed price or margin position is expired
          if( (_feed_price.valid() && cover_ask.get_price() > *_feed_price) )
          {
              _current_ask = cover_ask;
              --_collateral_itr;
              return true;
          }
          --_collateral_itr;
          break;
      }
      _collateral_itr.reset();
      break;
    }
    return false;
} FC_CAPTURE_AND_RETHROW() }

bool market_engine::get_next_ask_expired_cover()
{ try {
    /**
     *  Process expired collateral positions.
     *  Expired margin positions take second priority based upon age
     */
    while( _collateral_expiration_itr != _db_impl._collateral_expiration_index.end() )
    {
       if( _collateral_expiration_itr->quote_id != _quote_id )
          break;

       if( _collateral_expiration_itr->expiration > fc::time_point(_pending_state->now()) )
          break;

       auto val = _pending_state->get_collateral_record( _collateral_expiration_itr->key ); //_db_impl._collateral_db.fetch( _collateral_expiration_itr->key );
       if( !val || !val->collateral_balance )
       {
          ++_collateral_expiration_itr;
          continue;
       }

       const market_order cover_ask = build_collateral_market_order( _collateral_expiration_itr->key );
       ++_collateral_expiration_itr;

       // if we have a feed price and margin was called above then don't process it
       if( !(_feed_price.valid() && cover_ask.get_price() > *_feed_price) )
       {
          _current_ask = cover_ask;
          return true;
       } // else continue to next item
    }
    return false;
} FC_CAPTURE_AND_RETHROW() }

bool market_engine::get_next_ask_order()
{ try {
    /**
     *  Process asks.
     */

    if( !_ask_itr.valid() )
        return false;

    market_order abs_ask = market_order( ask_order, _ask_itr.key(), _ask_itr.value() );
    if( (abs_ask.get_price().quote_asset_id != _quote_id) || (abs_ask.get_price().base_asset_id != _base_id) )
        return false;

    _current_ask = abs_ask;
    ++_ask_itr;

    return true;
} FC_CAPTURE_AND_RETHROW() }

/**
  *  This method should not affect market execution or validation and
  *  is for historical purposes only.
  */
void market_engine::update_market_history( const asset& base_volume,
                                           const asset& quote_volume,
                                           const price& highest_price,
                                           const price& lowest_price,
                                           const price& opening_price,
                                           const price& closing_price,
                                           const fc::time_point_sec timestamp )
{ try {
    if( base_volume.amount == 0 && quote_volume.amount == 0)
        return;

    // Remark: only prices of matched orders be updated to market history
    market_history_key key(_quote_id, _base_id, market_history_key::each_block, _db_impl._head_block_header.timestamp);
    market_history_record new_record( highest_price, lowest_price, opening_price, closing_price,
                                      base_volume.amount, quote_volume.amount );

    //LevelDB iterators are dumb and don't support proper past-the-end semantics.
    auto last_key_itr = _db_impl._market_history_db.lower_bound( key );
    if( !last_key_itr.valid() ) last_key_itr = _db_impl._market_history_db.last();
    else --last_key_itr;

    key.timestamp = timestamp;

    // Unless the previous record for this market is the same as ours...
    // TODO check here: the previous commit checks for volume and prices change here,
    //                  I replaced them with key comparison, but looks odd as well.
    //                  maybe need to remove the judgements at all? since volume info is
    //                  always needed to be updated to market history,
    //                  even if prices and volumes are same to last block.
    if( !last_key_itr.valid() || last_key_itr.key() != key )
    {
        //...add a new entry to the history table.
        _pending_state->market_history[ key ] = new_record;
    }

    fc::time_point_sec start_of_this_hour = timestamp - (timestamp.sec_since_epoch() % (60*60));
    market_history_key old_key(_quote_id, _base_id, market_history_key::each_hour, start_of_this_hour);
    if( auto opt = _db_impl._market_history_db.fetch_optional(old_key) )
    {
        auto old_record = *opt;
        old_record.base_volume += new_record.base_volume;
        old_record.quote_volume += new_record.quote_volume;
        old_record.closing_price = new_record.closing_price;
        if( new_record.highest_bid > old_record.highest_bid || new_record.lowest_ask < old_record.lowest_ask )
        {
            old_record.highest_bid = std::max(new_record.highest_bid, old_record.highest_bid);
            old_record.lowest_ask = std::min(new_record.lowest_ask, old_record.lowest_ask);
        }
        // always update old data since volume changed
        _pending_state->market_history[old_key] = old_record;
    }
    else
    {
        _pending_state->market_history[old_key] = new_record;
    }

    fc::time_point_sec start_of_this_day = timestamp - (timestamp.sec_since_epoch() % (60*60*24));
    old_key = market_history_key(_quote_id, _base_id, market_history_key::each_day, start_of_this_day);
    if( auto opt = _db_impl._market_history_db.fetch_optional(old_key) )
    {
        auto old_record = *opt;
        old_record.base_volume += new_record.base_volume;
        old_record.quote_volume += new_record.quote_volume;
        old_record.closing_price = new_record.closing_price;
        if( new_record.highest_bid > old_record.highest_bid || new_record.lowest_ask < old_record.lowest_ask )
        {
            old_record.highest_bid = std::max(new_record.highest_bid, old_record.highest_bid);
            old_record.lowest_ask = std::min(new_record.lowest_ask, old_record.lowest_ask);
        }
        // always update old data since volume changed
        _pending_state->market_history[old_key] = old_record;
    }
    else
    {
        _pending_state->market_history[old_key] = new_record;
    }
} FC_CAPTURE_AND_RETHROW( (base_volume)(quote_volume)(highest_price)(lowest_price)(opening_price)(closing_price)(timestamp) ) }

asset market_engine::get_interest_paid( const asset& total_amount_paid, const price& apr, uint32_t age_seconds )
{ try {
    // TOTAL_PAID = DELTA_PRINCIPLE + DELTA_PRINCIPLE * APR * PERCENT_OF_YEAR
    // DELTA_PRINCIPLE = TOTAL_PAID / (1 + APR*PERCENT_OF_YEAR)
    // INTEREST_PAID  = TOTAL_PAID - DELTA_PRINCIPLE
    fc::real128 total_paid( total_amount_paid.amount );
    fc::real128 apr_n( (asset( BTS_BLOCKCHAIN_MAX_SHARES, apr.base_asset_id ) * apr).amount );
    fc::real128 apr_d( (asset( BTS_BLOCKCHAIN_MAX_SHARES, apr.base_asset_id ) ).amount );
    fc::real128 iapr = apr_n / apr_d;
    fc::real128 age_sec(age_seconds);
    fc::real128 sec_per_year(365 * 24 * 60 * 60);
    fc::real128 percent_of_year = age_sec / sec_per_year;

    fc::real128 delta_principle = total_paid / ( fc::real128(1) + iapr * percent_of_year );
    fc::real128 interest_paid   = total_paid - delta_principle;

    return asset( interest_paid.to_uint64(), total_amount_paid.asset_id );
} FC_CAPTURE_AND_RETHROW( (total_amount_paid)(apr)(age_seconds) ) }

asset market_engine::get_interest_owed( const asset& principle, const price& apr, uint32_t age_seconds )
{ try {
    // INTEREST_OWED = TOTAL_PRINCIPLE * APR * PERCENT_OF_YEAR
    fc::real128 total_principle( principle.amount );
    fc::real128 apr_n( (asset( BTS_BLOCKCHAIN_MAX_SHARES, apr.base_asset_id ) * apr).amount );
    fc::real128 apr_d( (asset( BTS_BLOCKCHAIN_MAX_SHARES, apr.base_asset_id ) ).amount );
    fc::real128 iapr = apr_n / apr_d;
    fc::real128 age_sec(age_seconds);
    fc::real128 sec_per_year(365 * 24 * 60 * 60);
    fc::real128 percent_of_year = age_sec / sec_per_year;

    fc::real128 interest_owed   = total_principle * iapr * percent_of_year;

    return asset( interest_owed.to_uint64(), principle.asset_id );
} FC_CAPTURE_AND_RETHROW( (principle)(apr)(age_seconds) ) }

asset market_engine::get_current_cover_debt() const
{ try {
    FC_ASSERT( _current_ask->type == cover_order );
    FC_ASSERT( _current_ask->interest_rate.valid() );

    return get_interest_owed( _current_ask->get_balance(),
                              *_current_ask->interest_rate,
                              get_current_cover_age() ) + _current_ask->get_balance();
} FC_CAPTURE_AND_RETHROW() }

void market_engine::handle_liquidation( const price& liqudation_price )
{ try {
   FC_ASSERT( false, "Black Swan Detected - Liquidate Market Now" );
   // Drop / Cancel all market orders that have executed so far
   // for each USD cover order, subtract liquidation_price*(debt+interest) from collateral and refund balance
   //    - pay interest to yield fund
   // calculate the yield per USD owed on all USD balances
   // for each USD balance record, set to 0 and issue (balance * yield_per_usd) * liquidation_price to owner
   //   - for escrow transactions convert escrow balance to USD
   // for each USD pending sell, cancel and issue balance * yield_per_usd * liquidation_price to owner
} FC_CAPTURE_AND_RETHROW( (liqudation_price) ) }

} } } // end namespace bts::blockchain::detail
