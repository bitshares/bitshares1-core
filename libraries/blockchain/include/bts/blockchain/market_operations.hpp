#pragma once

#include <bts/blockchain/market_records.hpp>
#include <bts/blockchain/operations.hpp>

namespace bts { namespace blockchain {

struct bid_operation
{
    static const operation_type_enum type;

    bid_operation():amount(0){}

    asset get_amount()const { return asset( amount, bid_index.order_price.quote_asset_id ); }

    share_type       amount;
    market_index_key bid_index;

    void evaluate( transaction_evaluation_state& eval_state )const;
};

struct ask_operation
{
    static const operation_type_enum type;

    ask_operation():amount(0){}

    asset get_amount()const { return asset( amount, ask_index.order_price.base_asset_id ); }

    share_type        amount;
    market_index_key  ask_index;

    void evaluate( transaction_evaluation_state& eval_state )const;
};

struct short_operation
{
    static const operation_type_enum type;

    short_operation():amount(0){}

    asset get_amount()const { return asset( amount, short_index.order_price.base_asset_id ); }

    share_type             amount;
    market_index_key_ext   short_index;

    void evaluate( transaction_evaluation_state& eval_state )const;
};

struct cover_operation
{
    static const operation_type_enum type;

    cover_operation():amount(0){}
    cover_operation( share_type a, const market_index_key& idx )
    :amount(a),cover_index(idx){}

    asset get_amount()const { return asset( amount, cover_index.order_price.quote_asset_id ); }

    share_type          amount;
    market_index_key    cover_index;
    fc::optional<price> reserved;

    void evaluate( transaction_evaluation_state& eval_state )const;
};

/**
*  Increases the collateral and lowers call price *if* the
*  new minimal call price is higher than the old call price.
*/
struct add_collateral_operation
{
    static const operation_type_enum type;

    add_collateral_operation(share_type amount = 0, market_index_key cover_index = market_index_key())
        : amount(amount), cover_index(cover_index){}

    asset get_amount()const { return asset( amount, cover_index.order_price.base_asset_id ); }

    share_type       amount;
    market_index_key cover_index;

    void evaluate( transaction_evaluation_state& eval_state )const;
};

} } // bts::blockchain

FC_REFLECT( bts::blockchain::bid_operation, (amount)(bid_index) )
FC_REFLECT( bts::blockchain::ask_operation, (amount)(ask_index) )
FC_REFLECT( bts::blockchain::short_operation, (amount)(short_index) )
FC_REFLECT( bts::blockchain::cover_operation, (amount)(cover_index)(reserved) )
FC_REFLECT( bts::blockchain::add_collateral_operation, (amount)(cover_index) )
