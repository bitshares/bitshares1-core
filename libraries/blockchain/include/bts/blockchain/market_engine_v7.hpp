#include <bts/blockchain/chain_database_impl.hpp>

namespace bts { namespace blockchain { namespace detail {

  class market_engine_v7
  {
  public:
    market_engine_v7( const pending_chain_state_ptr ps, const chain_database_impl& cdi );
    /** return true if execute was successful and applied */
    bool execute( asset_id_type quote_id, asset_id_type base_id, const fc::time_point_sec timestamp );

    void cancel_high_apr_shorts();

    static asset get_interest_paid_fixed(const asset& total_amount_paid, const price& apr, uint32_t age_seconds);
    static asset get_interest_owed_fixed(const asset& principle, const price& apr, uint32_t age_seconds);

    static asset get_interest_paid(const asset& total_amount_paid, const price& apr, uint32_t age_seconds);
    static asset get_interest_owed(const asset& principle, const price& apr, uint32_t age_seconds);

    static asset get_interest_paid_v1(const asset& total_amount_paid, const price& apr, uint32_t age_seconds);
    static asset get_interest_owed_v1(const asset& principle, const price& apr, uint32_t age_seconds);

  private:
    void push_market_transaction( const market_transaction& mtrx );

    void pay_current_short( market_transaction& mtrx,
                            asset_record& quote_asset,
                            asset_record& base_asset );
    void pay_current_bid( market_transaction& mtrx, asset_record& quote_asset );
    void pay_current_cover( market_transaction& mtrx, asset_record& quote_asset );
    void pay_current_ask( market_transaction& mtrx, asset_record& base_asset );

    bool get_next_short_v063();
    bool get_next_bid_v063();

    bool get_next_short_v064();
    bool get_next_bid_v064();

    bool get_next_short_v065( const omarket_order& bid_being_considered );
    bool get_next_bid_v065();

    bool get_next_short( const omarket_order& bid_being_considered = omarket_order() );
    bool get_next_bid();

    bool get_next_ask();
    asset get_current_cover_debt()const;
    uint32_t get_current_cover_age()const
    {
        //Total lifetime minus remaining lifetime
        return BTS_BLOCKCHAIN_MAX_SHORT_PERIOD_SEC - (*_current_ask->expiration - _pending_state->now()).to_seconds();
    }

    price minimum_ask()const
    {
        FC_ASSERT( _feed_price.valid() );
        price min_ask = *_feed_price;
        min_ask.ratio *= 9;
        min_ask.ratio /= 10;
        return min_ask;
    }

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

    void cancel_current_short( market_transaction& mtrx, const asset_id_type quote_asset_id );

    pending_chain_state_ptr       _pending_state;
    pending_chain_state_ptr       _prior_state;
    const chain_database_impl&    _db_impl;

    optional<market_order>        _current_bid;
    optional<market_order>        _current_ask;
    collateral_record             _current_collat_record;

    asset_id_type                 _quote_id;
    asset_id_type                 _base_id;
    oprice                        _feed_price;

    int                           _orders_filled = 0;

  public:
    vector<market_transaction>    _market_transactions;

  private:
    bts::db::cached_level_map< market_index_key, order_record >::iterator       _bid_itr;
    bts::db::cached_level_map< market_index_key, order_record >::iterator       _ask_itr;
    bts::db::cached_level_map< market_index_key, order_record >::iterator       _short_itr;
    bts::db::cached_level_map< market_index_key, collateral_record >::iterator  _collateral_itr;

    map<market_index_key, order_record>                                         _stuck_shorts;
    map<pair<price, market_index_key>, order_record>                            _unstuck_shorts;

    map<market_index_key, order_record>::reverse_iterator                       _stuck_shorts_iter;
    map<pair<price, market_index_key>, order_record>::reverse_iterator          _unstuck_shorts_iter;
  };

} } } // end namespace bts::blockchain::detail
