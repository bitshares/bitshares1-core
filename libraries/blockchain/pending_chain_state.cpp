#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/pending_chain_state.hpp>
#include <bts/blockchain/time.hpp>
#include <fc/io/raw_variant.hpp>

namespace bts { namespace blockchain {

   pending_chain_state::pending_chain_state( chain_interface* prev_state ) : _prev_state( prev_state )
   {
       init();
   }

   pending_chain_state::pending_chain_state( const pending_chain_state& copy )
   {
       *this = copy;
       init();
   }

   void pending_chain_state::init()
   { try {
       init_property_db_interface();
       init_account_db_interface();
       init_asset_db_interface();
       init_slate_db_interface();
       init_balance_db_interface();
       init_transaction_db_interface();
       init_feed_db_interface();
       init_slot_db_interface();
   } FC_CAPTURE_AND_RETHROW() }

   void pending_chain_state::clear()
   { try {
        _property_id_to_record.clear();
        _property_id_remove.clear();

        _account_id_to_record.clear();
        _account_id_remove.clear();
        _account_name_to_id.clear();
        _account_address_to_id.clear();

        _asset_id_to_record.clear();
        _asset_id_remove.clear();
        _asset_symbol_to_id.clear();

        _slate_id_to_record.clear();
        _slate_id_remove.clear();

        _balance_id_to_record.clear();
        _balance_id_remove.clear();

        _transaction_id_to_record.clear();
        _transaction_id_remove.clear();
        _transaction_digests.clear();

        _feed_index_to_record.clear();
        _feed_index_remove.clear();

        _slot_index_to_record.clear();
        _slot_index_remove.clear();
        _slot_timestamp_to_delegate.clear();

        burns.clear();

        asks.clear();
        bids.clear();
        relative_asks.clear();
        relative_bids.clear();
        shorts.clear();
        collateral.clear();

        _dirty_markets.clear();

        market_transactions.clear();
        market_statuses.clear();
        market_history.clear();
   } FC_CAPTURE_AND_RETHROW() }

   void pending_chain_state::set_prev_state( chain_interface* prev_state )
   { try {
       FC_ASSERT( prev_state != nullptr );
       _prev_state = prev_state;
   } FC_CAPTURE_AND_RETHROW() }

   uint32_t pending_chain_state::get_head_block_num()const
   { try {
       if( _prev_state == nullptr ) return 1;
       return _prev_state->get_head_block_num();
   } FC_CAPTURE_AND_RETHROW() }

   fc::time_point_sec pending_chain_state::now()const
   { try {
       if( _prev_state == nullptr ) return get_slot_start_time( blockchain::now() );
       return _prev_state->now();
   } FC_CAPTURE_AND_RETHROW() }

   oprice pending_chain_state::get_active_feed_price( const asset_id_type quote_id, const asset_id_type base_id )const
   { try {
       FC_ASSERT( _prev_state != nullptr );
       return _prev_state->get_active_feed_price( quote_id, base_id );
   } FC_CAPTURE_AND_RETHROW( (quote_id)(base_id) ) }

   /** Apply changes from this pending state to the previous state */
   void pending_chain_state::apply_changes()const
   { try {
       FC_ASSERT( _prev_state != nullptr );

       apply_records( _property_id_to_record, _property_id_remove );
       apply_records( _account_id_to_record, _account_id_remove );
       apply_records( _asset_id_to_record, _asset_id_remove );
       apply_records( _slate_id_to_record, _slate_id_remove );
       apply_records( _balance_id_to_record, _balance_id_remove );
       apply_records( _transaction_id_to_record, _transaction_id_remove );
       apply_records( _slot_index_to_record, _slot_index_remove );
       
       for( const auto& item : bids )            _prev_state->store_bid_record( item.first, item.second );
       for( const auto& item : relative_bids )   _prev_state->store_relative_bid_record( item.first, item.second );
       for( const auto& item : asks )            _prev_state->store_ask_record( item.first, item.second );
       for( const auto& item : relative_asks )   _prev_state->store_relative_ask_record( item.first, item.second );
       for( const auto& item : shorts )          _prev_state->store_short_record( item.first, item.second );
       for( const auto& item : collateral )      _prev_state->store_collateral_record( item.first, item.second );
       for( const auto& item : market_history )  _prev_state->store_market_history_record( item.first, item.second );
       for( const auto& item : market_statuses ) _prev_state->store_market_status( item.second );
       for( const auto& item : burns ) _prev_state->store_burn_record( burn_record(item.first,item.second) );
       
       /** do this last because it could have side effects on other records while
        * we manage the short index 
        */
       //apply_records( _prev_state, _feed_index_to_record, _feed_index_remove );
       for( auto item : _feed_index_to_record )
          _prev_state->store_feed_record( item.second );
       for( const auto& item : _feed_index_remove ) _prev_state->remove<feed_record>( item );
       
       _prev_state->set_market_transactions( market_transactions );
       _prev_state->set_dirty_markets( _dirty_markets );
   } FC_CAPTURE_AND_RETHROW() }

   otransaction_record pending_chain_state::get_transaction( const transaction_id_type& trx_id, bool exact )const
   { try {
       return lookup<transaction_record>( trx_id );
   } FC_CAPTURE_AND_RETHROW( (trx_id)(exact) ) }

   bool pending_chain_state::is_known_transaction( const transaction& trx )const
   { try {
       if( _transaction_digests.count( trx.digest( get_chain_id() ) ) > 0 ) return true;
       if( _prev_state == nullptr ) return false;
       return _prev_state->is_known_transaction( trx );
   } FC_CAPTURE_AND_RETHROW( (trx) ) }

   void pending_chain_state::store_transaction( const transaction_id_type& id, const transaction_record& rec )
   { try {
       store( id, rec );
   } FC_CAPTURE_AND_RETHROW( (id)(rec) ) }

   void pending_chain_state::build_undo_state( pending_chain_state& undo_state )const
   { try {
       FC_ASSERT( _prev_state != nullptr );

       populate_undo_state( undo_state, _property_id_to_record, _property_id_remove );
       populate_undo_state( undo_state, _account_id_to_record, _account_id_remove );
       populate_undo_state( undo_state, _asset_id_to_record, _asset_id_remove );
       populate_undo_state( undo_state, _slate_id_to_record, _slate_id_remove );
       populate_undo_state( undo_state, _balance_id_to_record, _balance_id_remove );
       populate_undo_state( undo_state, _transaction_id_to_record, _transaction_id_remove );
       populate_undo_state( undo_state, _feed_index_to_record, _feed_index_remove );
       populate_undo_state( undo_state, _slot_index_to_record, _slot_index_remove );
 
       for( const auto& item : bids )
       {
          auto prev_value = _prev_state->get_bid_record( item.first );
          if( prev_value.valid() ) undo_state.store_bid_record( item.first, *prev_value );
          else  undo_state.store_bid_record( item.first, order_record() );
       }
       for( const auto& item : relative_bids )
       {
          auto prev_value = _prev_state->get_relative_bid_record( item.first );
          if( prev_value.valid() ) undo_state.store_relative_bid_record( item.first, *prev_value );
          else  undo_state.store_relative_bid_record( item.first, order_record() );
       }
       for( const auto& item : asks )
       {
          auto prev_value = _prev_state->get_ask_record( item.first );
          if( prev_value.valid() ) undo_state.store_ask_record( item.first, *prev_value );
          else  undo_state.store_ask_record( item.first, order_record() );
       }
       for( const auto& item : relative_asks )
       {
          auto prev_value = _prev_state->get_relative_ask_record( item.first );
          if( prev_value.valid() ) undo_state.store_relative_ask_record( item.first, *prev_value );
          else  undo_state.store_relative_ask_record( item.first, order_record() );
       }
       for( const auto& item : shorts )
       {
          auto prev_value = _prev_state->get_short_record( item.first );
          if( prev_value.valid() ) undo_state.store_short_record( item.first, *prev_value );
          else  undo_state.store_short_record( item.first, order_record() );
       }
       for( const auto& item : collateral )
       {
          auto prev_value = _prev_state->get_collateral_record( item.first );
          if( prev_value.valid() ) undo_state.store_collateral_record( item.first, *prev_value );
          else  undo_state.store_collateral_record( item.first, collateral_record() );
       }
       for( const auto& item : market_statuses )
       {
          auto prev_value = _prev_state->get_market_status( item.first.first, item.first.second );
          if( prev_value ) undo_state.store_market_status( *prev_value );
          else
          {
             undo_state.store_market_status( market_status() );
          }
       }
       for( const auto& item : burns )
       {
          undo_state.store_burn_record( burn_record( item.first ) );
       }
 
       const auto dirty_markets = _prev_state->get_dirty_markets();
       undo_state.set_dirty_markets( dirty_markets );
   } FC_CAPTURE_AND_RETHROW( (undo_state) ) }

   /** load the state from a variant */
   void pending_chain_state::from_variant( const fc::variant& v )
   { try {
       fc::from_variant( v, *this );
   } FC_CAPTURE_AND_RETHROW( (v) ) }

   /** convert the state to a variant */
   fc::variant pending_chain_state::to_variant()const
   { try {
       fc::variant v;
       fc::to_variant( *this, v );
       return v;
   } FC_CAPTURE_AND_RETHROW() }

   oorder_record pending_chain_state::get_bid_record( const market_index_key& key )const
   {
      auto rec_itr = bids.find( key );
      if( rec_itr != bids.end() ) return rec_itr->second;
      else if( _prev_state ) return _prev_state->get_bid_record( key );
      return oorder_record();
   }
   oorder_record pending_chain_state::get_relative_bid_record( const market_index_key& key )const
   {
      auto rec_itr = relative_bids.find( key );
      if( rec_itr != relative_bids.end() ) return rec_itr->second;
      else if( _prev_state ) return _prev_state->get_relative_bid_record( key );
      return oorder_record();
   }

   omarket_order pending_chain_state::get_lowest_ask_record( const asset_id_type quote_id, const asset_id_type base_id )
   {
      omarket_order result;
      if( _prev_state )
      {
        auto pending = _prev_state->get_lowest_ask_record( quote_id, base_id );
        if( pending )
        {
           pending->state = *get_ask_record( pending->market_index );
        }
        return pending;
      }
      return result;
   }

   oorder_record pending_chain_state::get_ask_record( const market_index_key& key )const
   {
      auto rec_itr = asks.find( key );
      if( rec_itr != asks.end() ) return rec_itr->second;
      else if( _prev_state ) return _prev_state->get_ask_record( key );
      return oorder_record();
   }

   oorder_record pending_chain_state::get_relative_ask_record( const market_index_key& key )const
   {
      auto rec_itr = relative_asks.find( key );
      if( rec_itr != relative_asks.end() ) return rec_itr->second;
      else if( _prev_state ) return _prev_state->get_relative_ask_record( key );
      return oorder_record();
   }

   oorder_record pending_chain_state::get_short_record( const market_index_key& key )const
   {
      auto rec_itr = shorts.find( key );
      if( rec_itr != shorts.end() ) return rec_itr->second;
      else if( _prev_state ) return _prev_state->get_short_record( key );
      return oorder_record();
   }

   ocollateral_record pending_chain_state::get_collateral_record( const market_index_key& key )const
   {
      auto rec_itr = collateral.find( key );
      if( rec_itr != collateral.end() ) return rec_itr->second;
      else if( _prev_state ) return _prev_state->get_collateral_record( key );
      return ocollateral_record();
   }

   void pending_chain_state::store_bid_record( const market_index_key& key, const order_record& rec )
   {
      bids[ key ] = rec;
      _dirty_markets.insert( key.order_price.asset_pair() );
   }

   void pending_chain_state::store_ask_record( const market_index_key& key, const order_record& rec )
   {
      asks[ key ] = rec;
      _dirty_markets.insert( key.order_price.asset_pair() );
   }

   void pending_chain_state::store_relative_bid_record( const market_index_key& key, const order_record& rec )
   {
      relative_bids[ key ] = rec;
      _dirty_markets.insert( key.order_price.asset_pair() );
   }

   void pending_chain_state::store_relative_ask_record( const market_index_key& key, const order_record& rec )
   {
      relative_asks[ key ] = rec;
      _dirty_markets.insert( key.order_price.asset_pair() );
   }

   void pending_chain_state::store_short_record( const market_index_key& key, const order_record& rec )
   {
      shorts[ key ] = rec;
      _dirty_markets.insert( key.order_price.asset_pair() );
   }

   void pending_chain_state::set_market_dirty( const asset_id_type quote_id, const asset_id_type base_id )
   {
      _dirty_markets.insert( std::make_pair( quote_id, base_id ) );
   }

   void pending_chain_state::store_collateral_record( const market_index_key& key, const collateral_record& rec )
   {
      collateral[ key ] = rec;
      _dirty_markets.insert( key.order_price.asset_pair() );
   }

   void pending_chain_state::store_market_history_record(const market_history_key& key, const market_history_record& record)
   {
     market_history[ key ] = record;
   }

   omarket_history_record pending_chain_state::get_market_history_record(const market_history_key& key) const
   {
     if( market_history.find(key) != market_history.end() )
       return market_history.find(key)->second;
     return omarket_history_record();
   }

   void pending_chain_state::set_market_transactions( vector<market_transaction> trxs )
   {
      market_transactions = std::move(trxs);
   }

   omarket_status pending_chain_state::get_market_status( const asset_id_type quote_id, const asset_id_type base_id )
   {
      auto itr = market_statuses.find( std::make_pair(quote_id,base_id) );
      if( itr != market_statuses.end() )
         return itr->second;
      return _prev_state->get_market_status(quote_id,base_id);
   }

   void pending_chain_state::store_market_status( const market_status& s )
   {
      market_statuses[std::make_pair(s.quote_id,s.base_id)] = s;
   }

   void pending_chain_state::store_burn_record( const burn_record& br )
   {
      burns[br] = br;
   }

   oburn_record pending_chain_state::fetch_burn_record( const burn_record_key& key )const
   {
      auto itr = burns.find(key);
      if( itr == burns.end() )
      {
         return _prev_state->fetch_burn_record( key );
      }
      return burn_record( itr->first, itr->second );
   }

   void pending_chain_state::init_property_db_interface()
   {
       property_db_interface& interface = _property_db_interface;

       interface.lookup_by_id = [ this ]( const property_id_type id ) -> oproperty_record
       {
           const auto iter = _property_id_to_record.find( id );
           if( iter != _property_id_to_record.end() ) return iter->second;
           if( _property_id_remove.count( id ) > 0 ) return oproperty_record();
           if( !_prev_state ) return oproperty_record();
           return _prev_state->lookup<property_record>( id );
       };

       interface.insert_into_id_map = [ this ]( const property_id_type id, const property_record& record )
       {
           _property_id_remove.erase( id );
           _property_id_to_record[ id ] = record;
       };

       interface.erase_from_id_map = [ this ]( const property_id_type id )
       {
           _property_id_to_record.erase( id );
           _property_id_remove.insert( id );
       };
   }

   void pending_chain_state::init_account_db_interface()
   {
       account_db_interface& interface = _account_db_interface;

       interface.lookup_by_id = [ this ]( const account_id_type id ) -> oaccount_record
       {
           const auto iter = _account_id_to_record.find( id );
           if( iter != _account_id_to_record.end() ) return iter->second;
           if( _account_id_remove.count( id ) > 0 ) return oaccount_record();
           if( !_prev_state ) return oaccount_record();
           return _prev_state->lookup<account_record>( id );
       };

       interface.lookup_by_name = [ this ]( const string& name ) -> oaccount_record
       {
           const auto iter = _account_name_to_id.find( name );
           if( iter != _account_name_to_id.end() ) return _account_id_to_record.at( iter->second );
           if( !_prev_state ) return oaccount_record();
           const oaccount_record record = _prev_state->lookup<account_record>( name );
           if( record.valid() && _account_id_remove.count( record->id ) == 0 ) return *record;
           return oaccount_record();
       };

       interface.lookup_by_address = [ this ]( const address& addr ) -> oaccount_record
       {
           const auto iter = _account_address_to_id.find( addr );
           if( iter != _account_address_to_id.end() ) return _account_id_to_record.at( iter->second );
           if( !_prev_state ) return oaccount_record();
           const oaccount_record record = _prev_state->lookup<account_record>( addr );
           if( record.valid() && _account_id_remove.count( record->id ) == 0 ) return *record;
           return oaccount_record();
       };

       interface.insert_into_id_map = [ this ]( const account_id_type id, const account_record& record )
       {
           _account_id_remove.erase( id );
           _account_id_to_record[ id ] = record;
       };

       interface.insert_into_name_map = [ this ]( const string& name, const account_id_type id )
       {
           _account_name_to_id[ name ] = id;
       };

       interface.insert_into_address_map = [ this ]( const address& addr, const account_id_type id )
       {
           _account_address_to_id[ addr ] = id;
       };

       interface.insert_into_vote_set = [ this ]( const vote_del& vote )
       {
       };

       interface.erase_from_id_map = [ this ]( const account_id_type id )
       {
           _account_id_to_record.erase( id );
           _account_id_remove.insert( id );
       };

       interface.erase_from_name_map = [ this ]( const string& name )
       {
           _account_name_to_id.erase( name );
       };

       interface.erase_from_address_map = [ this ]( const address& addr )
       {
           _account_address_to_id.erase( addr );
       };

       interface.erase_from_vote_set = [ this ]( const vote_del& vote )
       {
       };
   }

   void pending_chain_state::init_asset_db_interface()
   {
       asset_db_interface& interface = _asset_db_interface;

       interface.lookup_by_id = [ this ]( const asset_id_type id ) -> oasset_record
       {
           const auto iter = _asset_id_to_record.find( id );
           if( iter != _asset_id_to_record.end() ) return iter->second;
           if( _asset_id_remove.count( id ) > 0 ) return oasset_record();
           if( !_prev_state ) return oasset_record();
           return _prev_state->lookup<asset_record>( id );
       };

       interface.lookup_by_symbol = [ this ]( const string& symbol ) -> oasset_record
       {
           const auto iter = _asset_symbol_to_id.find( symbol );
           if( iter != _asset_symbol_to_id.end() ) return _asset_id_to_record.at( iter->second );
           if( !_prev_state ) return oasset_record();
           const oasset_record record = _prev_state->lookup<asset_record>( symbol );
           if( record.valid() && _asset_id_remove.count( record->id ) == 0 ) return *record;
           return oasset_record();
       };

       interface.insert_into_id_map = [ this ]( const asset_id_type id, const asset_record& record )
       {
           _asset_id_remove.erase( id );
           _asset_id_to_record[ id ] = record;
       };

       interface.insert_into_symbol_map = [ this ]( const string& symbol, const asset_id_type id )
       {
           _asset_symbol_to_id[ symbol ] = id;
       };

       interface.erase_from_id_map = [ this ]( const asset_id_type id )
       {
           _asset_id_to_record.erase( id );
           _asset_id_remove.insert( id );
       };

       interface.erase_from_symbol_map = [ this ]( const string& symbol )
       {
           _asset_symbol_to_id.erase( symbol );
       };
   }

   void pending_chain_state::init_slate_db_interface()
   {
       slate_db_interface& interface = _slate_db_interface;

       interface.lookup_by_id = [ this ]( const slate_id_type id ) -> oslate_record
       {
           const auto iter = _slate_id_to_record.find( id );
           if( iter != _slate_id_to_record.end() ) return iter->second;
           if( _slate_id_remove.count( id ) > 0 ) return oslate_record();
           if( !_prev_state ) return oslate_record();
           return _prev_state->lookup<slate_record>( id );
       };

       interface.insert_into_id_map = [ this ]( const slate_id_type id, const slate_record& record )
       {
           _slate_id_remove.erase( id );
           _slate_id_to_record[ id ] = record;
       };

       interface.erase_from_id_map = [ this ]( const slate_id_type id )
       {
           _slate_id_to_record.erase( id );
           _slate_id_remove.insert( id );
       };
   }

   void pending_chain_state::init_balance_db_interface()
   {
       balance_db_interface& interface = _balance_db_interface;

       interface.lookup_by_id = [ this ]( const balance_id_type& id ) -> obalance_record
       {
           const auto iter = _balance_id_to_record.find( id );
           if( iter != _balance_id_to_record.end() ) return iter->second;
           if( _balance_id_remove.count( id ) > 0 ) return obalance_record();
           if( !_prev_state ) return obalance_record();
           return _prev_state->lookup<balance_record>( id );
       };

       interface.insert_into_id_map = [ this ]( const balance_id_type& id, const balance_record& record )
       {
           _balance_id_remove.erase( id );
           _balance_id_to_record[ id ] = record;
       };

       interface.erase_from_id_map = [ this ]( const balance_id_type& id )
       {
           _balance_id_to_record.erase( id );
           _balance_id_remove.insert( id );
       };
   }

   void pending_chain_state::init_transaction_db_interface()
   {
       transaction_db_interface& interface = _transaction_db_interface;

       interface.lookup_by_id = [ this ]( const transaction_id_type& id ) -> otransaction_record
       {
           const auto iter = _transaction_id_to_record.find( id );
           if( iter != _transaction_id_to_record.end() ) return iter->second;
           if( _transaction_id_remove.count( id ) > 0 ) return otransaction_record();
           if( !_prev_state ) return otransaction_record();
           return _prev_state->lookup<transaction_record>( id );
       };

       interface.insert_into_id_map = [ this ]( const transaction_id_type& id, const transaction_record& record )
       {
           _transaction_id_remove.erase( id );
           _transaction_id_to_record[ id ] = record;
       };

       interface.insert_into_unique_set = [ this ]( const transaction& trx )
       {
           _transaction_digests.insert( trx.digest( get_chain_id() ) );
       };

       interface.erase_from_id_map = [ this ]( const transaction_id_type& id )
       {
           _transaction_id_to_record.erase( id );
           _transaction_id_remove.insert( id );
       };

       interface.erase_from_unique_set = [ this ]( const transaction& trx )
       {
           _transaction_digests.erase( trx.digest( get_chain_id() ) );
       };
   }

   void pending_chain_state::init_feed_db_interface()
   {
       feed_db_interface& interface = _feed_db_interface;

       interface.lookup_by_index = [ this ]( const feed_index index ) -> ofeed_record
       {
           const auto iter = _feed_index_to_record.find( index );
           if( iter != _feed_index_to_record.end() ) return iter->second;
           if( _feed_index_remove.count( index ) > 0 ) return ofeed_record();
           if( !_prev_state ) return ofeed_record();
           return _prev_state->lookup<feed_record>( index );
       };

       interface.insert_into_index_map = [ this ]( const feed_index index, const feed_record& record )
       {
           _feed_index_remove.erase( index );
           _feed_index_to_record[ index ] = record;
       };

       interface.erase_from_index_map = [ this ]( const feed_index index )
       {
           _feed_index_to_record.erase( index );
           _feed_index_remove.insert( index );
       };
   }

   void pending_chain_state::init_slot_db_interface()
   {
       slot_db_interface& interface = _slot_db_interface;

       interface.lookup_by_index = [ this ]( const slot_index index ) -> oslot_record
       {
           const auto iter = _slot_index_to_record.find( index );
           if( iter != _slot_index_to_record.end() ) return iter->second;
           if( _slot_index_remove.count( index ) > 0 ) return oslot_record();
           if( !_prev_state ) return oslot_record();
           return _prev_state->lookup<slot_record>( index );
       };

       interface.lookup_by_timestamp = [ this ]( const time_point_sec timestamp ) -> oslot_record
       {
           const auto iter = _slot_timestamp_to_delegate.find( timestamp );
           if( iter != _slot_timestamp_to_delegate.end() ) return _slot_index_to_record.at( slot_index( iter->second, timestamp ) );
           if( !_prev_state ) return oslot_record();
           const oslot_record record = _prev_state->lookup<slot_record>( timestamp );
           if( record.valid() && _slot_index_remove.count( record->index ) == 0 ) return *record;
           return oslot_record();
       };

       interface.insert_into_index_map = [ this ]( const slot_index index, const slot_record& record )
       {
           _slot_index_remove.erase( index );
           _slot_index_to_record[ index ] = record;
       };

       interface.insert_into_timestamp_map = [ this ]( const time_point_sec timestamp, const account_id_type delegate_id )
       {
           _slot_timestamp_to_delegate[ timestamp ] = delegate_id;
       };

       interface.erase_from_index_map = [ this ]( const slot_index index )
       {
           _slot_index_to_record.erase( index );
           _slot_index_remove.insert( index );
       };

       interface.erase_from_timestamp_map = [ this ]( const time_point_sec timestamp )
       {
           _slot_timestamp_to_delegate.erase( timestamp );
       };
   }

} } // bts::blockchain
