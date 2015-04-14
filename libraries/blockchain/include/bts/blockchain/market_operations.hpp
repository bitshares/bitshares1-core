#pragma once

#include <bts/blockchain/market_records.hpp>
#include <bts/blockchain/operations.hpp>

namespace bts { namespace blockchain {

#include "market_operations_v1.hpp"

struct market_index_key_ext : public market_index_key
{
    market_index_key_ext( const market_index_key b = market_index_key(), optional<price> limit = optional<price>() )
        : market_index_key( b ), limit_price( limit ) {}

    optional<price> limit_price;

    friend bool operator == ( const market_index_key_ext& a, const market_index_key_ext& b )
    {
        if( a.limit_price.valid() != b.limit_price.valid() ) return false;
        if( !a.limit_price.valid() ) return market_index_key( a ) == market_index_key( b );
        return std::tie( a.order_price, a.owner, *a.limit_price ) == std::tie( b.order_price, b.owner, *b.limit_price );
    }

    friend bool operator < ( const market_index_key_ext& a, const market_index_key_ext& b )
    {
        FC_ASSERT( a.limit_price.valid() == b.limit_price.valid() );
        if( !a.limit_price.valid() ) return market_index_key( a ) < market_index_key( b );
        return std::tie( a.order_price, a.owner, *a.limit_price ) < std::tie( b.order_price, b.owner, *b.limit_price );
    }
};

struct bid_operation
{
    static const operation_type_enum type;

    bid_operation():amount(0){}

    asset get_amount()const { return asset( amount, bid_index.order_price.quote_asset_id ); }

    share_type       amount;
    market_index_key bid_index;

    void evaluate( transaction_evaluation_state& eval_state )const;
    void evaluate_v1( transaction_evaluation_state& eval_state )const;
};

struct ask_operation
{
    static const operation_type_enum type;

    ask_operation():amount(0){}

    asset get_amount()const { return asset( amount, ask_index.order_price.base_asset_id ); }

    share_type        amount;
    market_index_key  ask_index;

    void evaluate( transaction_evaluation_state& eval_state )const;
    void evaluate_v2( transaction_evaluation_state& eval_state )const;
    void evaluate_v1( transaction_evaluation_state& eval_state )const;
};

struct short_operation
{
    static const operation_type_enum type;

    short_operation():amount(0){}

    asset get_amount()const { return asset( amount, short_index.order_price.base_asset_id ); }

    share_type             amount;
    market_index_key_ext   short_index;

    void evaluate( transaction_evaluation_state& eval_state )const;
    void evaluate_v3( transaction_evaluation_state& eval_state )const;
    void evaluate_v2( transaction_evaluation_state& eval_state )const;
    void evaluate_v1( transaction_evaluation_state& eval_state )const;
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
    void evaluate_v5( transaction_evaluation_state& eval_state )const;
    void evaluate_v4( transaction_evaluation_state& eval_state )const;
    void evaluate_v3( transaction_evaluation_state& eval_state )const;
    void evaluate_v2( transaction_evaluation_state& eval_state )const;
    void evaluate_v1( transaction_evaluation_state& eval_state )const;
};

/**
*  Increases the collateral and lowers call price *if* the
*  new minimal call price is higher than the old call price.
*/
struct add_collateral_operation
{
    static const operation_type_enum type;

    add_collateral_operation(share_type amount = 0, market_index_key cover_index = market_index_key())
        : amount( amount ), cover_index( cover_index ) {}

    asset get_amount()const { return asset( amount, cover_index.order_price.base_asset_id ); }

    share_type       amount;
    market_index_key cover_index;

    void evaluate( transaction_evaluation_state& eval_state )const;
    void evaluate_v2( transaction_evaluation_state& eval_state )const;
    void evaluate_v1( transaction_evaluation_state& eval_state )const;
};

} } // bts::blockchain

FC_REFLECT_DERIVED( bts::blockchain::market_index_key_ext, (bts::blockchain::market_index_key), (limit_price) )
FC_REFLECT( bts::blockchain::bid_operation, (amount)(bid_index) )
FC_REFLECT( bts::blockchain::ask_operation, (amount)(ask_index) )
FC_REFLECT( bts::blockchain::short_operation, (amount)(short_index) )
FC_REFLECT( bts::blockchain::short_operation_v1, (amount)(short_index) )
FC_REFLECT( bts::blockchain::cover_operation, (amount)(cover_index)(reserved) )
FC_REFLECT( bts::blockchain::add_collateral_operation, (amount)(cover_index) )
