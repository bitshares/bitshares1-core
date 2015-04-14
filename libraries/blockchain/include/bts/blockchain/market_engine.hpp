#include <bts/blockchain/chain_database_impl.hpp>

namespace bts { namespace blockchain { namespace detail {

class market_engine
{
public:
    market_engine( const pending_chain_state_ptr ps, const chain_database_impl& cdi );

    bool execute( const asset_id_type quote_id, const asset_id_type base_id, const time_point_sec timestamp );

    static asset get_interest_paid( const asset& total_amount_paid, const price& apr, uint32_t age_seconds );
    static asset get_interest_owed( const asset& principle, const price& apr, uint32_t age_seconds );

private:
    void push_market_transaction( const market_transaction& mtrx );

    void pay_current_bid( market_transaction& mtrx, asset_record& quote_record, asset_record& base_record );
    void pay_current_short( market_transaction& mtrx, asset_record& quote_record, asset_record& base_record );
    void pay_current_ask( market_transaction& mtrx, asset_record& quote_record, asset_record& base_record );
    void pay_current_cover( market_transaction& mtrx, asset_record& quote_record );

    bool get_next_bid();
    bool get_next_short( const omarket_order& bid_being_considered = omarket_order() );

    bool get_next_ask();
    bool get_next_ask_margin_call();
    bool get_next_ask_expired_cover();
    bool get_next_ask_order();

    asset get_current_cover_debt()const;
    uint32_t get_current_cover_age()const
    {
        // Total lifetime minus remaining lifetime
        return BTS_BLOCKCHAIN_MAX_SHORT_PERIOD_SEC - (*_current_ask->expiration - _pending_state->now()).to_seconds();
    }

    price minimum_cover_ask_price()const
    {
        FC_ASSERT( _feed_price.valid() );
        price min_ask = *_feed_price;
        min_ask.ratio *= 9;
        min_ask.ratio /= 10;
        return min_ask;
    }
    
    market_order build_collateral_market_order( market_index_key k ) const;
    
    void handle_liquidation( const price& liqudation_price );

    /**
    *  This method should not affect market execution or validation and
    *  is for historical purposes only.
    */
    void update_market_history( const asset& base_volume,
                                const asset& quote_volume,
                                const price& highest_price,
                                const price& lowest_price,
                                const price& opening_price,
                                const price& closing_price,
                                const fc::time_point_sec timestamp );

    pending_chain_state_ptr     _pending_state;
    pending_chain_state_ptr     _prior_state;
    const chain_database_impl&  _db_impl;

    optional<market_order>      _current_bid;
    optional<market_order>      _current_ask;

    asset_id_type               _quote_id;
    asset_id_type               _base_id;
    oprice                      _feed_price;

    int                         _orders_filled = 0;
    int                         _current_pass = 0;

public:
    vector<market_transaction>    _market_transactions;

private:
    bts::db::cached_level_map<market_index_key, order_record>::iterator         _bid_itr;
    bts::db::cached_level_map<market_index_key, order_record>::iterator         _ask_itr;
    bts::db::cached_level_map<market_index_key, collateral_record>::iterator    _collateral_itr;

    map<market_index_key, order_record>                                         _stuck_shorts;
    map<pair<price, market_index_key>, order_record>                            _unstuck_shorts;

    map<market_index_key, order_record>::reverse_iterator                       _stuck_shorts_iter;
    map<pair<price, market_index_key>, order_record>::reverse_iterator          _unstuck_shorts_iter;

    std::set<expiration_index>::iterator                                        _collateral_expiration_itr;
};

} } } // end namespace bts::blockchain::detail
