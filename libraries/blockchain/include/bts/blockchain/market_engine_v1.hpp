#include <bts/blockchain/chain_database_impl.hpp>

namespace bts { namespace blockchain { namespace detail {

  class market_engine_v1
  {
  public:
    market_engine_v1( const pending_chain_state_ptr ps, const chain_database_impl& cdi );
    bool execute( asset_id_type quote_id, asset_id_type base_id, const fc::time_point_sec timestamp );

  private:
    bool get_next_bid();
    bool get_next_ask();

    pending_chain_state_ptr       _pending_state;
    pending_chain_state_ptr       _prior_state;
    const chain_database_impl&    _db_impl;

    optional<market_order>        _current_bid;
    optional<market_order>        _current_ask;
    share_type                    _current_payoff_balance = 0;
    asset_id_type                 _quote_id;
    asset_id_type                 _base_id;
    status_record                 _market_stat;

  public:
    vector<market_transaction>    _market_transactions;

  private:
    bts::db::cached_level_map< market_index_key, order_record >::iterator         _bid_itr;
    bts::db::cached_level_map< market_index_key, order_record >::iterator         _ask_itr;
    bts::db::cached_level_map< market_index_key, order_record >::iterator         _short_itr;
    bts::db::cached_level_map< market_index_key, collateral_record >::iterator    _collateral_itr;
  };

} } } // end namespace bts::blockchain::detail
