#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/pending_chain_state.hpp>
#include <bts/blockchain/time.hpp>
#include <fc/io/raw_variant.hpp>

namespace bts { namespace blockchain {

   pending_chain_state::pending_chain_state( chain_interface_ptr prev_state )
   : _prev_state( prev_state )
   {
   }

   void pending_chain_state::set_prev_state( chain_interface_ptr prev_state )
   {
      _prev_state = prev_state;
   }

   uint32_t pending_chain_state::get_head_block_num()const
   {
      const chain_interface_ptr prev_state = _prev_state.lock();
      if( !prev_state ) return 1;
      return prev_state->get_head_block_num();
   }

   fc::time_point_sec pending_chain_state::now()const
   {
      const chain_interface_ptr prev_state = _prev_state.lock();
      if( !prev_state ) return get_slot_start_time( blockchain::now() );
      return prev_state->now();
   }

   oprice pending_chain_state::get_active_feed_price( const asset_id_type quote_id )const
   {
      const chain_interface_ptr prev_state = _prev_state.lock();
      FC_ASSERT( prev_state );
      return prev_state->get_active_feed_price( quote_id );
   }

   void pending_chain_state::apply_changes()const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      if( !prev_state ) return;

      apply_records( prev_state, _property_id_to_record, _property_id_remove );
      apply_records( prev_state, _account_id_to_record, _account_id_remove );
      apply_records( prev_state, _asset_id_to_record, _asset_id_remove );
      apply_records( prev_state, _slate_id_to_record, _slate_id_remove );
      apply_records( prev_state, _balance_id_to_record, _balance_id_remove );
      apply_records( prev_state, _transaction_id_to_record, _transaction_id_remove );
      apply_records( prev_state, _burn_index_to_record, _burn_index_remove );
      apply_records( prev_state, _status_index_to_record, _status_index_remove );
      apply_records( prev_state, _feed_index_to_record, _feed_index_remove );
      apply_records( prev_state, _slot_index_to_record, _slot_index_remove );

      for( const auto& item : bids )            prev_state->store_bid_record( item.first, item.second );
      for( const auto& item : asks )            prev_state->store_ask_record( item.first, item.second );
      for( const auto& item : shorts )          prev_state->store_short_record( item.first, item.second );
      for( const auto& item : collateral )      prev_state->store_collateral_record( item.first, item.second );
      for( const auto& item : market_history )  prev_state->store_market_history_record( item.first, item.second );

      prev_state->set_market_transactions( market_transactions );
      prev_state->set_dirty_markets( _dirty_markets );
   }

   otransaction_record pending_chain_state::get_transaction( const transaction_id_type& trx_id, bool exact )const
   {
       return lookup<transaction_record>( trx_id );
   }

   bool pending_chain_state::is_known_transaction( const transaction& trx )const
   { try {
       if( _transaction_digests.count( trx.digest( get_chain_id() ) ) > 0 ) return true;
       chain_interface_ptr prev_state = _prev_state.lock();
       if( prev_state ) return prev_state->is_known_transaction( trx );
       return false;
   } FC_CAPTURE_AND_RETHROW( (trx) ) }

   void pending_chain_state::store_transaction( const transaction_id_type& id, const transaction_record& rec )
   {
       store( id, rec );
   }

   void pending_chain_state::build_undo_state( const chain_interface_ptr& undo_state_arg )const
   {
      auto undo_state = std::dynamic_pointer_cast<pending_chain_state>( undo_state_arg );
      chain_interface_ptr prev_state = _prev_state.lock();
      FC_ASSERT( prev_state );

      populate_undo_state( undo_state, prev_state, _property_id_to_record, _property_id_remove );
      populate_undo_state( undo_state, prev_state, _account_id_to_record, _account_id_remove );
      populate_undo_state( undo_state, prev_state, _asset_id_to_record, _asset_id_remove );
      populate_undo_state( undo_state, prev_state, _slate_id_to_record, _slate_id_remove );
      populate_undo_state( undo_state, prev_state, _balance_id_to_record, _balance_id_remove );
      populate_undo_state( undo_state, prev_state, _transaction_id_to_record, _transaction_id_remove );
      populate_undo_state( undo_state, prev_state, _burn_index_to_record, _burn_index_remove );
      populate_undo_state( undo_state, prev_state, _status_index_to_record, _status_index_remove );
      populate_undo_state( undo_state, prev_state, _feed_index_to_record, _feed_index_remove );
      populate_undo_state( undo_state, prev_state, _slot_index_to_record, _slot_index_remove );

      for( const auto& item : bids )
      {
         auto prev_value = prev_state->get_bid_record( item.first );
         if( prev_value.valid() ) undo_state->store_bid_record( item.first, *prev_value );
         else  undo_state->store_bid_record( item.first, order_record() );
      }
      for( const auto& item : asks )
      {
         auto prev_value = prev_state->get_ask_record( item.first );
         if( prev_value.valid() ) undo_state->store_ask_record( item.first, *prev_value );
         else  undo_state->store_ask_record( item.first, order_record() );
      }
      for( const auto& item : shorts )
      {
         auto prev_value = prev_state->get_short_record( item.first );
         if( prev_value.valid() ) undo_state->store_short_record( item.first, *prev_value );
         else  undo_state->store_short_record( item.first, order_record() );
      }
      for( const auto& item : collateral )
      {
         auto prev_value = prev_state->get_collateral_record( item.first );
         if( prev_value.valid() ) undo_state->store_collateral_record( item.first, *prev_value );
         else  undo_state->store_collateral_record( item.first, collateral_record() );
      }

      const auto dirty_markets = prev_state->get_dirty_markets();
      undo_state->set_dirty_markets( dirty_markets );
   }

   void pending_chain_state::from_variant( const fc::variant& v )
   {
      fc::from_variant( v, *this );
   }

   fc::variant pending_chain_state::to_variant()const
   {
      fc::variant v;
      fc::to_variant( *this, v );
      return v;
   }

   oorder_record pending_chain_state::get_bid_record( const market_index_key& key )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto rec_itr = bids.find( key );
      if( rec_itr != bids.end() ) return rec_itr->second;
      else if( prev_state ) return prev_state->get_bid_record( key );
      return oorder_record();
   }

   omarket_order pending_chain_state::get_lowest_ask_record( const asset_id_type quote_id, const asset_id_type base_id )
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      omarket_order result;
      if( prev_state )
      {
        auto pending = prev_state->get_lowest_ask_record( quote_id, base_id );
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
      chain_interface_ptr prev_state = _prev_state.lock();
      auto rec_itr = asks.find( key );
      if( rec_itr != asks.end() ) return rec_itr->second;
      else if( prev_state ) return prev_state->get_ask_record( key );
      return oorder_record();
   }

   oorder_record pending_chain_state::get_short_record( const market_index_key& key )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto rec_itr = shorts.find( key );
      if( rec_itr != shorts.end() ) return rec_itr->second;
      else if( prev_state ) return prev_state->get_short_record( key );
      return oorder_record();
   }

   ocollateral_record pending_chain_state::get_collateral_record( const market_index_key& key )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto rec_itr = collateral.find( key );
      if( rec_itr != collateral.end() ) return rec_itr->second;
      else if( prev_state ) return prev_state->get_collateral_record( key );
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

   void pending_chain_state::check_supplies()const
   {
       unordered_map<asset_id_type, share_type> deltas;

       const chain_interface_ptr prev_state = _prev_state.lock();
       FC_ASSERT( prev_state );

       for( const auto& item : _account_id_to_record )
       {
           const account_id_type id = item.first;
           const oaccount_record prev_record = prev_state->get_account_record( id );
           if( prev_record.valid() )
           {
               if( prev_record->delegate_info.valid() )
                   deltas[ 0 ] -= prev_record->delegate_info->pay_balance;
           }

           const account_record& record = item.second;
           if( record.delegate_info.valid() )
               deltas[ 0 ] += record.delegate_info->pay_balance;
       }
       for( const account_id_type id : _account_id_remove )
       {
           const oaccount_record prev_record = prev_state->get_account_record( id );
           if( prev_record.valid() )
           {
               if( prev_record->delegate_info.valid() )
                   deltas[ 0 ] -= prev_record->delegate_info->pay_balance;
           }
       }

       for( const auto& item : _asset_id_to_record )
       {
           const asset_id_type id = item.first;
           const oasset_record prev_record = prev_state->get_asset_record( id );
           if( prev_record.valid() )
               deltas[ prev_record->id ] -= prev_record->collected_fees;

           const asset_record& record = item.second;
           deltas[ record.id ] += record.collected_fees;
       }
       for( const asset_id_type id : _asset_id_remove )
       {
           const oasset_record prev_record = prev_state->get_asset_record( id );
           if( prev_record.valid() )
               deltas[ prev_record->id ] -= prev_record->collected_fees;
       }

       for( const auto& item : _balance_id_to_record )
       {
           const balance_id_type& id = item.first;
           const obalance_record prev_record = prev_state->get_balance_record( id );
           if( prev_record.valid() )
               deltas[ prev_record->asset_id() ] -= prev_record->balance;

           const balance_record& record = item.second;
           deltas[ record.asset_id() ] += record.balance;
       }
       for( const balance_id_type& id : _balance_id_remove )
       {
           const obalance_record prev_record = prev_state->get_balance_record( id );
           if( prev_record.valid() )
               deltas[ prev_record->asset_id() ] -= prev_record->balance;
       }

       for( const auto& item : asks )
       {
           const market_index_key& index = item.first;
           const oorder_record prev_record = prev_state->get_ask_record( index );
           if( prev_record.valid() )
               deltas[ index.order_price.base_asset_id ] -= prev_record->balance;

           const order_record& record = item.second;
           if( !record.is_null() )
               deltas[ index.order_price.base_asset_id ] += record.balance;
       }

       for( const auto& item : bids )
       {
           const market_index_key& index = item.first;
           const oorder_record prev_record = prev_state->get_bid_record( index );
           if( prev_record.valid() )
               deltas[ index.order_price.quote_asset_id ] -= prev_record->balance;

           const order_record& record = item.second;
           if( !record.is_null() )
               deltas[ index.order_price.quote_asset_id ] += record.balance;
       }

       for( const auto& item : shorts )
       {
           const market_index_key& index = item.first;
           const oorder_record prev_record = prev_state->get_short_record( index );
           if( prev_record.valid() )
               deltas[ 0 ] -= prev_record->balance;

           const order_record& record = item.second;
           if( !record.is_null() )
               deltas[ 0 ] += record.balance;
       }

       for( const auto& item : collateral )
       {
           const market_index_key& index = item.first;
           const ocollateral_record prev_record = prev_state->get_collateral_record( index );
           if( prev_record.valid() )
               deltas[ 0 ] -= prev_record->collateral_balance;

           const collateral_record& record = item.second;
           if( !record.is_null() )
               deltas[ 0 ] += record.collateral_balance;
       }

       for( const auto& item : deltas )
       {
           const asset_id_type id = item.first;
           const share_type actual_delta = item.second;
           share_type reported_delta = 0;

           const oasset_record prev_record = prev_state->get_asset_record( id );
           if( prev_record.valid() )
               reported_delta -= prev_record->current_supply;

           const oasset_record record = get_asset_record( id );
           if( record.valid() )
               reported_delta += record->current_supply;

           if( reported_delta != actual_delta )
               FC_CAPTURE_AND_THROW( unexpected_supply_change, (id)(actual_delta)(reported_delta) );
       }
   }

   oproperty_record pending_chain_state::property_lookup_by_id( const property_id_type id )const
   {
       const auto iter = _property_id_to_record.find( id );
       if( iter != _property_id_to_record.end() ) return iter->second;
       if( _property_id_remove.count( id ) > 0 ) return oproperty_record();
       const chain_interface_ptr prev_state = _prev_state.lock();
       if( !prev_state ) return oproperty_record();
       return prev_state->lookup<property_record>( id );
   }

   void pending_chain_state::property_insert_into_id_map( const property_id_type id, const property_record& record )
   {
       _property_id_remove.erase( id );
       _property_id_to_record[ id ] = record;
   }

   void pending_chain_state::property_erase_from_id_map( const property_id_type id )
   {
       _property_id_to_record.erase( id );
       _property_id_remove.insert( id );
   }

   oaccount_record pending_chain_state::account_lookup_by_id( const account_id_type id )const
   {
       const auto iter = _account_id_to_record.find( id );
       if( iter != _account_id_to_record.end() ) return iter->second;
       if( _account_id_remove.count( id ) > 0 ) return oaccount_record();
       const chain_interface_ptr prev_state = _prev_state.lock();
       if( !prev_state ) return oaccount_record();
       return prev_state->lookup<account_record>( id );
   }

   oaccount_record pending_chain_state::account_lookup_by_name( const string& name )const
   {
       const auto iter = _account_name_to_id.find( name );
       if( iter != _account_name_to_id.end() ) return _account_id_to_record.at( iter->second );
       const chain_interface_ptr prev_state = _prev_state.lock();
       if( !prev_state ) return oaccount_record();
       const oaccount_record record = prev_state->lookup<account_record>( name );
       if( record.valid() && _account_id_remove.count( record->id ) == 0 ) return *record;
       return oaccount_record();
   }

   oaccount_record pending_chain_state::account_lookup_by_address( const address& addr )const
   {
       const auto iter = _account_address_to_id.find( addr );
       if( iter != _account_address_to_id.end() ) return _account_id_to_record.at( iter->second );
       const chain_interface_ptr prev_state = _prev_state.lock();
       if( !prev_state ) return oaccount_record();
       const oaccount_record record = prev_state->lookup<account_record>( addr );
       if( record.valid() && _account_id_remove.count( record->id ) == 0 ) return *record;
       return oaccount_record();
   }

   void pending_chain_state::account_insert_into_id_map( const account_id_type id, const account_record& record )
   {
       _account_id_remove.erase( id );
       _account_id_to_record[ id ] = record;
   }

   void pending_chain_state::account_insert_into_name_map( const string& name, const account_id_type id )
   {
       _account_name_to_id[ name ] = id;
   }

   void pending_chain_state::account_insert_into_address_map( const address& addr, const account_id_type id )
   {
       _account_address_to_id[ addr ] = id;
   }

   void pending_chain_state::account_insert_into_vote_set( const vote_del& )
   {
   }

   void pending_chain_state::account_erase_from_id_map( const account_id_type id )
   {
       _account_id_to_record.erase( id );
       _account_id_remove.insert( id );
   }

   void pending_chain_state::account_erase_from_name_map( const string& name )
   {
       _account_name_to_id.erase( name );
   }

   void pending_chain_state::account_erase_from_address_map( const address& addr )
   {
       _account_address_to_id.erase( addr );
   }

   void pending_chain_state::account_erase_from_vote_set( const vote_del& )
   {
   }

   oasset_record pending_chain_state::asset_lookup_by_id( const asset_id_type id )const
   {
       const auto iter = _asset_id_to_record.find( id );
       if( iter != _asset_id_to_record.end() ) return iter->second;
       if( _asset_id_remove.count( id ) > 0 ) return oasset_record();
       const chain_interface_ptr prev_state = _prev_state.lock();
       if( !prev_state ) return oasset_record();
       return prev_state->lookup<asset_record>( id );
   }

   oasset_record pending_chain_state::asset_lookup_by_symbol( const string& symbol )const
   {
       const auto iter = _asset_symbol_to_id.find( symbol );
       if( iter != _asset_symbol_to_id.end() ) return _asset_id_to_record.at( iter->second );
       const chain_interface_ptr prev_state = _prev_state.lock();
       if( !prev_state ) return oasset_record();
       const oasset_record record = prev_state->lookup<asset_record>( symbol );
       if( record.valid() && _asset_id_remove.count( record->id ) == 0 ) return *record;
       return oasset_record();
   }

   void pending_chain_state::asset_insert_into_id_map( const asset_id_type id, const asset_record& record )
   {
       _asset_id_remove.erase( id );
       _asset_id_to_record[ id ] = record;
   }

   void pending_chain_state::asset_insert_into_symbol_map( const string& symbol, const asset_id_type id )
   {
       _asset_symbol_to_id[ symbol ] = id;
   }

   void pending_chain_state::asset_erase_from_id_map( const asset_id_type id )
   {
       _asset_id_to_record.erase( id );
       _asset_id_remove.insert( id );
   }

   void pending_chain_state::asset_erase_from_symbol_map( const string& symbol )
   {
       _asset_symbol_to_id.erase( symbol );
   }

   oslate_record pending_chain_state::slate_lookup_by_id( const slate_id_type id )const
   {
       const auto iter = _slate_id_to_record.find( id );
       if( iter != _slate_id_to_record.end() ) return iter->second;
       if( _slate_id_remove.count( id ) > 0 ) return oslate_record();
       const chain_interface_ptr prev_state = _prev_state.lock();
       if( !prev_state ) return oslate_record();
       return prev_state->lookup<slate_record>( id );
   }

   void pending_chain_state::slate_insert_into_id_map( const slate_id_type id, const slate_record& record )
   {
       _slate_id_remove.erase( id );
       _slate_id_to_record[ id ] = record;
   }

   void pending_chain_state::slate_erase_from_id_map( const slate_id_type id )
   {
       _slate_id_to_record.erase( id );
       _slate_id_remove.insert( id );
   }

   obalance_record pending_chain_state::balance_lookup_by_id( const balance_id_type& id )const
   {
       const auto iter = _balance_id_to_record.find( id );
       if( iter != _balance_id_to_record.end() ) return iter->second;
       if( _balance_id_remove.count( id ) > 0 ) return obalance_record();
       const chain_interface_ptr prev_state = _prev_state.lock();
       if( !prev_state ) return obalance_record();
       return prev_state->lookup<balance_record>( id );
   }

   void pending_chain_state::balance_insert_into_id_map( const balance_id_type& id, const balance_record& record )
   {
       _balance_id_remove.erase( id );
       _balance_id_to_record[ id ] = record;
   }

   void pending_chain_state::balance_erase_from_id_map( const balance_id_type& id )
   {
       _balance_id_to_record.erase( id );
       _balance_id_remove.insert( id );
   }

   otransaction_record pending_chain_state::transaction_lookup_by_id( const transaction_id_type& id )const
   {
       const auto iter = _transaction_id_to_record.find( id );
       if( iter != _transaction_id_to_record.end() ) return iter->second;
       if( _transaction_id_remove.count( id ) > 0 ) return otransaction_record();
       const chain_interface_ptr prev_state = _prev_state.lock();
       if( !prev_state ) return otransaction_record();
       return prev_state->lookup<transaction_record>( id );
   }

   void pending_chain_state::transaction_insert_into_id_map( const transaction_id_type& id, const transaction_record& record )
   {
       _transaction_id_remove.erase( id );
       _transaction_id_to_record[ id ] = record;
   }

   void pending_chain_state::transaction_insert_into_unique_set( const transaction& trx )
   {
       _transaction_digests.insert( trx.digest( get_chain_id() ) );
   }

   void pending_chain_state::transaction_erase_from_id_map( const transaction_id_type& id )
   {
       _transaction_id_to_record.erase( id );
       _transaction_id_remove.insert( id );
   }

   void pending_chain_state::transaction_erase_from_unique_set( const transaction& trx )
   {
       _transaction_digests.erase( trx.digest( get_chain_id() ) );
   }

   oburn_record pending_chain_state::burn_lookup_by_index( const burn_index& index )const
   {
       const auto iter = _burn_index_to_record.find( index );
       if( iter != _burn_index_to_record.end() ) return iter->second;
       if( _burn_index_remove.count( index ) > 0 ) return oburn_record();
       const chain_interface_ptr prev_state = _prev_state.lock();
       if( !prev_state ) return oburn_record();
       return prev_state->lookup<burn_record>( index );
   }

   void pending_chain_state::burn_insert_into_index_map( const burn_index& index, const burn_record& record )
   {
       _burn_index_remove.erase( index );
       _burn_index_to_record[ index ] = record;
   }

   void pending_chain_state::burn_erase_from_index_map( const burn_index& index )
   {
       _burn_index_to_record.erase( index );
       _burn_index_remove.insert( index );
   }

   ostatus_record pending_chain_state::status_lookup_by_index( const status_index index )const
   {
       const auto iter = _status_index_to_record.find( index );
       if( iter != _status_index_to_record.end() ) return iter->second;
       if( _status_index_remove.count( index ) > 0 ) return ostatus_record();
       const chain_interface_ptr prev_state = _prev_state.lock();
       if( !prev_state ) return ostatus_record();
       return prev_state->lookup<status_record>( index );
   }

   void pending_chain_state::status_insert_into_index_map( const status_index index, const status_record& record )
   {
       _status_index_remove.erase( index );
       _status_index_to_record[ index ] = record;
   }

   void pending_chain_state::status_erase_from_index_map( const status_index index )
   {
       _status_index_to_record.erase( index );
       _status_index_remove.insert( index );
   }

   ofeed_record pending_chain_state::feed_lookup_by_index( const feed_index index )const
   {
       const auto iter = _feed_index_to_record.find( index );
       if( iter != _feed_index_to_record.end() ) return iter->second;
       if( _feed_index_remove.count( index ) > 0 ) return ofeed_record();
       const chain_interface_ptr prev_state = _prev_state.lock();
       if( !prev_state ) return ofeed_record();
       return prev_state->lookup<feed_record>( index );
   }

   void pending_chain_state::feed_insert_into_index_map( const feed_index index, const feed_record& record )
   {
       _feed_index_remove.erase( index );
       _feed_index_to_record[ index ] = record;
   }

   void pending_chain_state::feed_erase_from_index_map( const feed_index index )
   {
       _feed_index_to_record.erase( index );
       _feed_index_remove.insert( index );
   }

   oslot_record pending_chain_state::slot_lookup_by_index( const slot_index index )const
   {
       const auto iter = _slot_index_to_record.find( index );
       if( iter != _slot_index_to_record.end() ) return iter->second;
       if( _slot_index_remove.count( index ) > 0 ) return oslot_record();
       const chain_interface_ptr prev_state = _prev_state.lock();
       if( !prev_state ) return oslot_record();
       return prev_state->lookup<slot_record>( index );
   }

   oslot_record pending_chain_state::slot_lookup_by_timestamp( const time_point_sec timestamp )const
   {
       const auto iter = _slot_timestamp_to_delegate.find( timestamp );
       if( iter != _slot_timestamp_to_delegate.end() ) return _slot_index_to_record.at( slot_index( iter->second, timestamp ) );
       const chain_interface_ptr prev_state = _prev_state.lock();
       if( !prev_state ) return oslot_record();
       const oslot_record record = prev_state->lookup<slot_record>( timestamp );
       if( record.valid() && _slot_index_remove.count( record->index ) == 0 ) return *record;
       return oslot_record();
   }

   void pending_chain_state::slot_insert_into_index_map( const slot_index index, const slot_record& record )
   {
       _slot_index_remove.erase( index );
       _slot_index_to_record[ index ] = record;
   }

   void pending_chain_state::slot_insert_into_timestamp_map( const time_point_sec timestamp, const account_id_type delegate_id )
   {
       _slot_timestamp_to_delegate[ timestamp ] = delegate_id;
   }

   void pending_chain_state::slot_erase_from_index_map( const slot_index index )
   {
       _slot_index_to_record.erase( index );
       _slot_index_remove.insert( index );
   }

   void pending_chain_state::slot_erase_from_timestamp_map( const time_point_sec timestamp )
   {
       _slot_timestamp_to_delegate.erase( timestamp );
   }

} } // bts::blockchain
