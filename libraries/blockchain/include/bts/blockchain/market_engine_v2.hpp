#include <bts/blockchain/chain_database_impl.hpp>

namespace bts { namespace blockchain { namespace detail {

  class market_engine_v2
  {
  public:
    market_engine_v2( const pending_chain_state_ptr ps, const chain_database_impl& cdi );
    bool execute( asset_id_type quote_id, asset_id_type base_id, const fc::time_point_sec timestamp );

  private:
    void push_market_transaction( const market_transaction& mtrx );

    void pay_current_short( const market_transaction& mtrx, const asset& xts_paid_by_short, asset_record& quote_asset );
    void pay_current_bid( const market_transaction& mtrx, asset_record& quote_asset );
    void pay_current_cover( market_transaction& mtrx, asset_record& quote_asset, asset_record& base_asset );
    void pay_current_ask( const market_transaction& mtrx, asset_record& base_asset );

    bool get_next_bid();
    bool get_next_ask();

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

    void reset_current_ask();

    pending_chain_state_ptr       _pending_state;
    pending_chain_state_ptr       _prior_state;
    const chain_database_impl&    _db_impl;

    optional<market_order>        _current_bid;
    optional<market_order>        _current_ask;
    optional<market_order>        _current_ask_backup; // used to allow us to validate blocks before BTS_V0_4_10_FORK_BLOCK_NUM
    share_type                    _current_payoff_balance = 0;
    asset_id_type                 _quote_id;
    asset_id_type                 _base_id;

    int                           _orders_filled = 0;

  public:
    vector<market_transaction>    _market_transactions;

  private:
    bts::db::cached_level_map< market_index_key, order_record >::iterator         _bid_itr;
    bts::db::cached_level_map< market_index_key, order_record >::iterator         _ask_itr;
    bts::db::cached_level_map< market_index_key, order_record >::iterator         _short_itr;
    bts::db::cached_level_map< market_index_key, collateral_record >::iterator    _collateral_itr;
  };

} } } // end namespace bts::blockchain::detail
