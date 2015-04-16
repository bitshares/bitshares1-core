#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/market_engine.hpp>
#include <bts/blockchain/market_operations.hpp>
#include <bts/blockchain/pending_chain_state.hpp>

#include <fc/real128.hpp>
#include <algorithm>

#include <bts/blockchain/fork_blocks.hpp>

namespace bts { namespace blockchain {

/**
 *  If the amount is negative then it will withdraw/cancel the bid assuming
 *  it is signed by the owner and there is sufficient funds.
 *
 *  If the amount is positive then it will add funds to the bid.
 */
void bid_operation::evaluate( transaction_evaluation_state& eval_state )const
{ try {
    if( eval_state.pending_state()->get_head_block_num() < BTS_V0_9_0_FORK_BLOCK_NUM )
        return evaluate_v1( eval_state );

    if( this->amount == 0 )
        FC_CAPTURE_AND_THROW( zero_amount );

    const asset delta_amount = this->get_amount();

    const asset_id_type quote_asset_id = this->bid_index.order_price.quote_asset_id;
    const asset_id_type base_asset_id = this->bid_index.order_price.base_asset_id;

    const oasset_record quote_asset_record = eval_state.pending_state()->get_asset_record( quote_asset_id );
    const oasset_record base_asset_record = eval_state.pending_state()->get_asset_record( base_asset_id );
    if( !quote_asset_record.valid() || !base_asset_record.valid() )
        FC_CAPTURE_AND_THROW( invalid_price );

    const address& owner = this->bid_index.owner;

    oorder_record current_order = eval_state.pending_state()->get_bid_record( this->bid_index );

    if( delta_amount.amount > 0 ) // Withdraw from transaction and deposit to order
    {
        if( quote_asset_id <= base_asset_id )
            FC_CAPTURE_AND_THROW( invalid_price );

        if( this->bid_index.order_price.ratio <= 0 )
            FC_CAPTURE_AND_THROW( invalid_price );

        if( quote_asset_record->flag_is_active( asset_record::halted_markets )
            || base_asset_record->flag_is_active( asset_record::halted_markets ) )
        {
            FC_CAPTURE_AND_THROW( market_halted );
        }

        if( eval_state.pending_state()->get_head_block_num() >= BTS_V0_9_1_FORK_BLOCK_NUM )
            FC_ASSERT( base_asset_record->address_is_approved( *eval_state.pending_state(), owner ) );

        if( !current_order.valid() )
            current_order = order_record();

        eval_state.sub_balance( delta_amount );
    }
    else // Withdraw from order and deposit to transaction
    {
        if( !current_order.valid() )
            FC_CAPTURE_AND_THROW( unknown_market_order );

        if( -delta_amount.amount > current_order->balance )
            FC_CAPTURE_AND_THROW( insufficient_funds, (*current_order) );

        eval_state.add_balance( -delta_amount );
    }

    FC_ASSERT( current_order.valid() );
    current_order->balance += delta_amount.amount;
    current_order->last_update = eval_state.pending_state()->now();

    eval_state.pending_state()->store_bid_record( this->bid_index, *current_order );

    if( !eval_state.check_signature( owner ) && !quote_asset_record->authority_is_retracting( eval_state ) )
        FC_CAPTURE_AND_THROW( missing_signature );
} FC_CAPTURE_AND_RETHROW( (*this) ) }

/**
 *  If the amount is negative then it will withdraw/cancel the bid assuming
 *  it is signed by the owner and there is sufficient funds.
 *
 *  If the amount is positive then it will add funds to the bid.
 */
void ask_operation::evaluate( transaction_evaluation_state& eval_state )const
{ try {
    if( eval_state.pending_state()->get_head_block_num() < BTS_V0_9_0_FORK_BLOCK_NUM )
        return evaluate_v2( eval_state );

    if( this->amount == 0 )
        FC_CAPTURE_AND_THROW( zero_amount );

    const asset delta_amount = this->get_amount();

    const asset_id_type quote_asset_id = this->ask_index.order_price.quote_asset_id;
    const asset_id_type base_asset_id = this->ask_index.order_price.base_asset_id;

    const oasset_record quote_asset_record = eval_state.pending_state()->get_asset_record( quote_asset_id );
    const oasset_record base_asset_record = eval_state.pending_state()->get_asset_record( base_asset_id );
    if( !quote_asset_record.valid() || !base_asset_record.valid() )
        FC_CAPTURE_AND_THROW( invalid_price );

    const address& owner = this->ask_index.owner;

    oorder_record current_order = eval_state.pending_state()->get_ask_record( this->ask_index );

    if( delta_amount.amount > 0 ) // Withdraw from transaction and deposit to order
    {
        if( quote_asset_id <= base_asset_id )
            FC_CAPTURE_AND_THROW( invalid_price );

        if( this->ask_index.order_price.ratio <= 0 )
            FC_CAPTURE_AND_THROW( invalid_price );

        if( quote_asset_record->flag_is_active( asset_record::halted_markets )
            || base_asset_record->flag_is_active( asset_record::halted_markets ) )
        {
            FC_CAPTURE_AND_THROW( market_halted );
        }

        if( eval_state.pending_state()->get_head_block_num() >= BTS_V0_9_1_FORK_BLOCK_NUM )
            FC_ASSERT( quote_asset_record->address_is_approved( *eval_state.pending_state(), owner ) );

        if( !current_order.valid() )
            current_order = order_record();

        eval_state.sub_balance( delta_amount );
    }
    else // Withdraw from order and deposit to transaction
    {
        if( !current_order.valid() )
            FC_CAPTURE_AND_THROW( unknown_market_order );

        if( -delta_amount.amount > current_order->balance )
            FC_CAPTURE_AND_THROW( insufficient_funds, (*current_order) );

        eval_state.add_balance( -delta_amount );
    }

    FC_ASSERT( current_order.valid() );
    current_order->balance += delta_amount.amount;
    current_order->last_update = eval_state.pending_state()->now();

    eval_state.pending_state()->store_ask_record( this->ask_index, *current_order );

    if( !eval_state.check_signature( owner ) && !base_asset_record->authority_is_retracting( eval_state ) )
        FC_CAPTURE_AND_THROW( missing_signature );
} FC_CAPTURE_AND_RETHROW( (*this) ) }

void short_operation::evaluate( transaction_evaluation_state& eval_state )const
{ try {
    if( eval_state.pending_state()->get_head_block_num() < BTS_V0_9_0_FORK_BLOCK_NUM )
        return evaluate_v3( eval_state );

    if( this->amount == 0 )
        FC_CAPTURE_AND_THROW( zero_amount );

    const asset delta_amount = this->get_amount();

    const asset_id_type quote_asset_id = this->short_index.order_price.quote_asset_id;
    const asset_id_type base_asset_id = this->short_index.order_price.base_asset_id;

    const oasset_record quote_asset_record = eval_state.pending_state()->get_asset_record( quote_asset_id );
    const oasset_record base_asset_record = eval_state.pending_state()->get_asset_record( base_asset_id );
    if( !quote_asset_record.valid() || !base_asset_record.valid() )
        FC_CAPTURE_AND_THROW( invalid_apr );

    const address& owner = this->short_index.owner;

    oorder_record current_order = eval_state.pending_state()->get_short_record( this->short_index );

    if( delta_amount.amount > 0 ) // Withdraw from transaction and deposit to order
    {
        if( base_asset_id != asset_id_type( 0 ) )
            FC_CAPTURE_AND_THROW( invalid_apr );

        if( quote_asset_id <= base_asset_id )
            FC_CAPTURE_AND_THROW( invalid_apr );

        if( !quote_asset_record->is_market_issued() )
            FC_CAPTURE_AND_THROW( invalid_market );

        static const fc::uint128 max_apr = fc::uint128( BTS_BLOCKCHAIN_MAX_SHORT_APR_PCT ) * FC_REAL128_PRECISION / 100;
        if( this->short_index.order_price.ratio < fc::uint128( 0 ) || this->short_index.order_price.ratio > max_apr )
            FC_CAPTURE_AND_THROW( invalid_apr );

        if( this->short_index.limit_price.valid() )
        {
            const price& limit_price = *this->short_index.limit_price;
            if( limit_price.quote_asset_id != quote_asset_id || limit_price.base_asset_id != base_asset_id )
                FC_CAPTURE_AND_THROW( invalid_price );

            if( limit_price.ratio <= 0 )
                FC_CAPTURE_AND_THROW( invalid_price );
        }

        if( !current_order.valid() )
            current_order = order_record();

        eval_state.sub_balance( delta_amount );
    }
    else // Withdraw from order and deposit to transaction
    {
        if( !current_order.valid() )
            FC_CAPTURE_AND_THROW( unknown_market_order );

        if( -delta_amount.amount > current_order->balance )
            FC_CAPTURE_AND_THROW( insufficient_funds, (*current_order) );

        eval_state.add_balance( -delta_amount );
    }

    FC_ASSERT( current_order.valid() );
    current_order->balance += delta_amount.amount;
    current_order->limit_price = this->short_index.limit_price;
    current_order->last_update = eval_state.pending_state()->now();

    eval_state.pending_state()->store_short_record( this->short_index, *current_order );

    if( !eval_state.check_signature( owner ) )
        FC_CAPTURE_AND_THROW( missing_signature );
} FC_CAPTURE_AND_RETHROW( (*this) ) }

/**
  pay off part of the USD balance, if balance goes to 0 then close out
  the position and transfer collateral to proper place.
  update the call price (remove old value, add new value)
*/
void cover_operation::evaluate( transaction_evaluation_state& eval_state )const
{ try {
    if( eval_state.pending_state()->get_head_block_num() < BTS_V0_9_0_FORK_BLOCK_NUM )
        return evaluate_v5( eval_state );

    if( this->amount <= 0 )
        FC_CAPTURE_AND_THROW( invalid_amount );

    ocollateral_record current_position = eval_state.pending_state()->get_collateral_record( this->cover_index );
    if( !current_position.valid() )
        FC_CAPTURE_AND_THROW( unknown_market_order );

    const asset_id_type quote_asset_id = this->cover_index.order_price.quote_asset_id;
    oasset_record quote_asset_record = eval_state.pending_state()->get_asset_record( quote_asset_id );
    if( !quote_asset_record.valid() )
        FC_CAPTURE_AND_THROW( invalid_price );

    const asset delta_amount = this->get_amount();
    eval_state.sub_balance( delta_amount );

    const address& owner = this->cover_index.owner;

    const time_point_sec start_time = current_position->expiration - fc::seconds( BTS_BLOCKCHAIN_MAX_SHORT_PERIOD_SEC );
    const time_point_sec now = eval_state.pending_state()->now();
    const uint32_t elapsed_sec = ( start_time >= now ) ? 0 : ( now - start_time ).to_seconds();

    const asset principle = asset( current_position->payoff_balance, delta_amount.asset_id );
    const asset total_debt = detail::market_engine::get_interest_owed( principle, current_position->interest_rate, elapsed_sec )
                             + principle;

    asset principle_paid; // Principle will be destroyed
    asset interest_paid; // Interest goes to fees
    if( delta_amount >= total_debt )
    {
        // Payoff the whole debt
        principle_paid = principle;
        interest_paid = delta_amount - principle_paid; // Include any extra payment amount
        current_position->payoff_balance = 0;

        // Withdraw from position and deposit to transaction
        eval_state.add_balance( asset( current_position->collateral_balance, this->cover_index.order_price.base_asset_id ) );
    }
    else
    {
        // Partial cover
        interest_paid = detail::market_engine::get_interest_paid( delta_amount, current_position->interest_rate, elapsed_sec );
        principle_paid = delta_amount - interest_paid;
        current_position->payoff_balance -= principle_paid.amount;
        FC_ASSERT( current_position->payoff_balance > 0 );

        const price new_call_price = asset( current_position->payoff_balance, this->cover_index.order_price.quote_asset_id )
                                     / asset( (current_position->collateral_balance * BTS_BLOCKCHAIN_MCALL_D2C_NUMERATOR)
                                              / BTS_BLOCKCHAIN_MCALL_D2C_DENOMINATOR, this->cover_index.order_price.base_asset_id );

        eval_state.pending_state()->store_collateral_record( market_index_key( new_call_price, owner ), *current_position );
    }

    // Remove original position since it was either paid off or updated to a new call price
    eval_state.pending_state()->store_collateral_record( this->cover_index, collateral_record() );

    // Adjust debt asset supply
    FC_ASSERT( principle_paid.amount >= 0 && interest_paid.amount >= 0 );
    quote_asset_record->current_supply -= principle_paid.amount;
    quote_asset_record->collected_fees += interest_paid.amount;
    eval_state.pending_state()->store_asset_record( *quote_asset_record );

    if( !eval_state.check_signature( owner ) )
        FC_CAPTURE_AND_THROW( missing_signature );
} FC_CAPTURE_AND_RETHROW( (*this) ) }

void add_collateral_operation::evaluate( transaction_evaluation_state& eval_state )const
{ try {
    if( eval_state.pending_state()->get_head_block_num() < BTS_V0_9_0_FORK_BLOCK_NUM )
        return evaluate_v2( eval_state );

    if( this->amount <= 0 )
        FC_CAPTURE_AND_THROW( invalid_amount );

    ocollateral_record current_position = eval_state.pending_state()->get_collateral_record( this->cover_index );
    if( !current_position.valid() )
        FC_CAPTURE_AND_THROW( unknown_market_order );

    const asset delta_amount = this->get_amount();
    eval_state.sub_balance( delta_amount );

    const address& owner = this->cover_index.owner;

    current_position->collateral_balance += delta_amount.amount;

    const price new_call_price = asset( current_position->payoff_balance, this->cover_index.order_price.quote_asset_id )
                                 / asset( (current_position->collateral_balance * BTS_BLOCKCHAIN_MCALL_D2C_NUMERATOR)
                                          / BTS_BLOCKCHAIN_MCALL_D2C_DENOMINATOR, this->cover_index.order_price.base_asset_id );

    // Remove old position and insert new one
    eval_state.pending_state()->store_collateral_record( this->cover_index, collateral_record() );
    eval_state.pending_state()->store_collateral_record( market_index_key( new_call_price, owner ), *current_position );

    if( !eval_state.check_signature( owner ) )
        FC_CAPTURE_AND_THROW( missing_signature );
} FC_CAPTURE_AND_RETHROW( (*this) ) }

} } // bts::blockchain
