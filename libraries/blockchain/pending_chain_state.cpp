#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/pending_chain_state.hpp>
#include <bts/blockchain/time.hpp>
#include <fc/io/raw_variant.hpp>

namespace bts { namespace blockchain {

   pending_chain_state::pending_chain_state( chain_interface_ptr prev_state )
   :_prev_state( prev_state )
   {
      init();
   }
   pending_chain_state::pending_chain_state( const pending_chain_state& p )
   {
      *this = p;
      init();
   }

   void pending_chain_state::init()
   {
      init_property_db_interface();
      init_account_db_interface();
      init_asset_db_interface();
      init_slate_db_interface();
      init_balance_db_interface();
      init_transaction_db_interface();
      init_feed_db_interface();
      init_slot_db_interface();
   }

   pending_chain_state::~pending_chain_state()
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

   oprice pending_chain_state::get_active_feed_price( const asset_id_type quote_id, const asset_id_type base_id )const
   {
      const chain_interface_ptr prev_state = _prev_state.lock();
      FC_ASSERT( prev_state );
      return prev_state->get_active_feed_price( quote_id, base_id );
   }

   /** Apply changes from this pending state to the previous state */
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
      apply_records( prev_state, _slot_index_to_record, _slot_index_remove );

      for( const auto& item : authorizations )  prev_state->authorize( item.first.first, item.first.second, item.second );
      for( const auto& item : bids )            prev_state->store_bid_record( item.first, item.second );
      for( const auto& item : relative_bids )   prev_state->store_relative_bid_record( item.first, item.second );
      for( const auto& item : asks )            prev_state->store_ask_record( item.first, item.second );
      for( const auto& item : relative_asks )   prev_state->store_relative_ask_record( item.first, item.second );
      for( const auto& item : shorts )          prev_state->store_short_record( item.first, item.second );
      for( const auto& item : collateral )      prev_state->store_collateral_record( item.first, item.second );
      for( const auto& item : market_history )  prev_state->store_market_history_record( item.first, item.second );
      for( const auto& item : market_statuses ) prev_state->store_market_status( item.second );
      for( const auto& item : asset_proposals ) prev_state->store_asset_proposal( item.second );
      for( const auto& item : burns ) prev_state->store_burn_record( burn_record(item.first,item.second) );
      for( const auto& item : objects ) prev_state->store_object_record( item.second );

      /** do this last because it could have side effects on other records while
       * we manage the short index 
       */
      //apply_records( prev_state, _feed_index_to_record, _feed_index_remove );
      for( auto item : _feed_index_to_record )
         prev_state->store_feed_record( item.second );
      for( const auto& item : _feed_index_remove ) prev_state->remove<feed_record>( item );

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

   void pending_chain_state::get_undo_state( const chain_interface_ptr& undo_state_arg )const
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
      populate_undo_state( undo_state, prev_state, _feed_index_to_record, _feed_index_remove );
      populate_undo_state( undo_state, prev_state, _slot_index_to_record, _slot_index_remove );

      for( const auto& item : asset_proposals )
      {
         auto prev_value = prev_state->fetch_asset_proposal( item.first.first, item.first.second );
         if( !!prev_value ) undo_state->store_asset_proposal( *prev_value );
         else undo_state->store_asset_proposal( item.second.make_null() );
      }
      for( const auto& item : authorizations )
      {
         auto prev_value = prev_state->get_authorization( item.first.first, item.first.second );
         if( !!prev_value ) undo_state->authorize( item.first.first, item.first.second, *prev_value );
         else undo_state->deauthorize( item.first.first, item.first.second );
      }
      for( const auto& item : bids )
      {
         auto prev_value = prev_state->get_bid_record( item.first );
         if( prev_value.valid() ) undo_state->store_bid_record( item.first, *prev_value );
         else  undo_state->store_bid_record( item.first, order_record() );
      }
      for( const auto& item : relative_bids )
      {
         auto prev_value = prev_state->get_relative_bid_record( item.first );
         if( prev_value.valid() ) undo_state->store_relative_bid_record( item.first, *prev_value );
         else  undo_state->store_relative_bid_record( item.first, order_record() );
      }
      for( const auto& item : asks )
      {
         auto prev_value = prev_state->get_ask_record( item.first );
         if( prev_value.valid() ) undo_state->store_ask_record( item.first, *prev_value );
         else  undo_state->store_ask_record( item.first, order_record() );
      }
      for( const auto& item : relative_asks )
      {
         auto prev_value = prev_state->get_relative_ask_record( item.first );
         if( prev_value.valid() ) undo_state->store_relative_ask_record( item.first, *prev_value );
         else  undo_state->store_relative_ask_record( item.first, order_record() );
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
      for( const auto& item : market_statuses )
      {
         auto prev_value = prev_state->get_market_status( item.first.first, item.first.second );
         if( prev_value ) undo_state->store_market_status( *prev_value );
         else
         {
            undo_state->store_market_status( market_status() );
         }
      }
      for( const auto& item : burns )
      {
         undo_state->store_burn_record( burn_record( item.first ) );
      }
      for( const auto& item : objects )
      {
         undo_state->store_object_record( item.second );
      }

      const auto dirty_markets = prev_state->get_dirty_markets();
      undo_state->set_dirty_markets( dirty_markets );
   }

   /** load the state from a variant */
   void pending_chain_state::from_variant( const fc::variant& v )
   {
      fc::from_variant( v, *this );
   }

   /** convert the state to a variant */
   fc::variant pending_chain_state::to_variant()const
   {
      fc::variant v;
      fc::to_variant( *this, v );
      return v;
   }

   oobject_record pending_chain_state::get_object_record(const object_id_type id)const
   {
       chain_interface_ptr prev_state = _prev_state.lock();
       auto itr = objects.find( id );
       if( itr != objects.end() )
           return itr->second;
       else if( prev_state )
           return prev_state->get_object_record( id );
        return oobject_record();
   }

   void pending_chain_state::store_object_record(const object_record& obj)
   {
        ilog("@n storing object in pending_chain_state");
        // Set indices
        switch( obj.type() )
        {
            case account_object:
            case asset_object:
                FC_ASSERT(false, "You cannot store these object types via object interface yet!");
                break;
            case edge_object:
            {
                ilog("@n it is an edge");
                store_edge_record( obj );
                break;
            }
            case base_object:
            {
                ilog("@n it is a base object");
                objects[obj._id] = obj;
                break;
            }
            case site_object:
            {
                ilog("@n it is a site");
                auto site = obj.as<site_record>();
                store_site_record( site );
                break;
            }
            default:
                break;
        }

   }

    void                       pending_chain_state::store_edge_record( const object_record& edge )
    {
        ilog("@n existing edge before storing edge in pending state:");
        ilog("@n      as an object: ${o}", ("o", objects[edge._id]));
        ilog("@n      as an edge: ${e}", ("e", objects[edge._id].as<edge_record>()));
        auto edge_data = edge.as<edge_record>();
        edge_index[ edge_data.index_key() ] = edge._id;
        reverse_edge_index[ edge_data.reverse_index_key() ] = edge._id;
        objects[edge._id] = edge;
        ilog("@n after storing edge in pending state:");
        ilog("@n      as an object: ${o}", ("o", objects[edge._id]));
        ilog("@n      as an edge: ${e}", ("e", objects[edge._id].as<edge_record>()));
    }

    void                       pending_chain_state::store_site_record( const site_record& site )
    {
        FC_ASSERT(false, "unimplemented");
        /*
        site_index[site.site_name] = site;
        objects[site._id] = site;
        ilog("@n after storing site in pending state:");
        ilog("@n      as an object: ${o}", ("o", objects[site._id]));
        ilog("@n      as a site: ${s}", ("s", objects[site._id].as<site_record>()));
        */
    }

    oobject_record               pending_chain_state::get_edge( const object_id_type from,
                                         const object_id_type to,
                                         const string& name )const
    {
        edge_index_key key(from, to, name);
        auto itr = edge_index.find( key );
        if( itr == edge_index.end() )
            return oobject_record();
        auto oobj = get_object_record( itr->second );
        return oobj;
        /*
        auto obj = get_object_record( itr->second );
        FC_ASSERT(obj.valid(), "This edge was in the index, but it has no object record");
        return obj->as<edge_record>();
        */
    }
    map<string, object_record>   pending_chain_state::get_edges( const object_id_type from,
                                          const object_id_type to )const
    {
        FC_ASSERT(false, "unimplemented!");
    }
    map<object_id_type, map<string, object_record>> pending_chain_state::get_edges( const object_id_type from )const
    {
        FC_ASSERT(false, "unimplemented!");
    }

   osite_record  pending_chain_state::lookup_site( const string& site_name)const
   { try {
       auto prev_state = _prev_state.lock();
       auto itr = site_index.find( site_name );
       if( itr != site_index.end() )
       {
           return itr->second;
           /*
           auto site = get_object_record( itr->second );
           FC_ASSERT( site.valid(), "A new index was in the pending chain state, but the record was not there" );
           return site->as<site_record>();
           */
       }
       if( prev_state )
           return prev_state->lookup_site( site_name );
       return osite_record();
   } FC_CAPTURE_AND_RETHROW( (site_name) ) }

   oorder_record pending_chain_state::get_bid_record( const market_index_key& key )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto rec_itr = bids.find( key );
      if( rec_itr != bids.end() ) return rec_itr->second;
      else if( prev_state ) return prev_state->get_bid_record( key );
      return oorder_record();
   }
   oorder_record pending_chain_state::get_relative_bid_record( const market_index_key& key )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto rec_itr = relative_bids.find( key );
      if( rec_itr != relative_bids.end() ) return rec_itr->second;
      else if( prev_state ) return prev_state->get_relative_bid_record( key );
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

   oorder_record pending_chain_state::get_relative_ask_record( const market_index_key& key )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto rec_itr = relative_asks.find( key );
      if( rec_itr != relative_asks.end() ) return rec_itr->second;
      else if( prev_state ) return prev_state->get_relative_ask_record( key );
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
      chain_interface_ptr prev_state = _prev_state.lock();
      return prev_state->get_market_status(quote_id,base_id);
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
         chain_interface_ptr prev_state = _prev_state.lock();
         return prev_state->fetch_burn_record( key );
      }
      return burn_record( itr->first, itr->second );
   }

   void pending_chain_state::authorize( asset_id_type asset_id, const address& owner, object_id_type oid  )
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      authorizations[std::make_pair(asset_id,owner)] = oid;
   }

   optional<object_id_type> pending_chain_state::get_authorization( asset_id_type asset_id, const address& owner )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto index = std::make_pair( asset_id, owner );
      auto itr = authorizations.find( index );
      if( itr == authorizations.end() ) return prev_state->get_authorization( asset_id, owner );
      if( itr->second != -1 )
         return itr->second;
      return optional<object_id_type>();
   }

   void pending_chain_state::store_asset_proposal( const proposal_record& r )
   {
      asset_proposals[std::make_pair( r.asset_id, r.proposal_id )] = r;
   }

   optional<proposal_record> pending_chain_state::fetch_asset_proposal( asset_id_type asset_id, proposal_id_type proposal_id )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto itr = asset_proposals.find( std::make_pair( asset_id, proposal_id ) );
      if( itr != asset_proposals.end() ) return itr->second;
      return prev_state->fetch_asset_proposal( asset_id, proposal_id );
   }

   void pending_chain_state::init_property_db_interface()
   {
       property_db_interface& interface = _property_db_interface;

       interface.lookup_by_id = [ this ]( const property_id_type id ) -> oproperty_record
       {
           const auto iter = _property_id_to_record.find( id );
           if( iter != _property_id_to_record.end() ) return iter->second;
           if( _property_id_remove.count( id ) > 0 ) return oproperty_record();
           const chain_interface_ptr prev_state = _prev_state.lock();
           if( !prev_state ) return oproperty_record();
           return prev_state->lookup<property_record>( id );
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
           const chain_interface_ptr prev_state = _prev_state.lock();
           if( !prev_state ) return oaccount_record();
           return prev_state->lookup<account_record>( id );
       };

       interface.lookup_by_name = [ this ]( const string& name ) -> oaccount_record
       {
           const auto iter = _account_name_to_id.find( name );
           if( iter != _account_name_to_id.end() ) return _account_id_to_record.at( iter->second );
           const chain_interface_ptr prev_state = _prev_state.lock();
           if( !prev_state ) return oaccount_record();
           const oaccount_record record = prev_state->lookup<account_record>( name );
           if( record.valid() && _account_id_remove.count( record->id ) == 0 ) return *record;
           return oaccount_record();
       };

       interface.lookup_by_address = [ this ]( const address& addr ) -> oaccount_record
       {
           const auto iter = _account_address_to_id.find( addr );
           if( iter != _account_address_to_id.end() ) return _account_id_to_record.at( iter->second );
           const chain_interface_ptr prev_state = _prev_state.lock();
           if( !prev_state ) return oaccount_record();
           const oaccount_record record = prev_state->lookup<account_record>( addr );
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
           const chain_interface_ptr prev_state = _prev_state.lock();
           if( !prev_state ) return oasset_record();
           return prev_state->lookup<asset_record>( id );
       };

       interface.lookup_by_symbol = [ this ]( const string& symbol ) -> oasset_record
       {
           const auto iter = _asset_symbol_to_id.find( symbol );
           if( iter != _asset_symbol_to_id.end() ) return _asset_id_to_record.at( iter->second );
           const chain_interface_ptr prev_state = _prev_state.lock();
           if( !prev_state ) return oasset_record();
           const oasset_record record = prev_state->lookup<asset_record>( symbol );
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
           const chain_interface_ptr prev_state = _prev_state.lock();
           if( !prev_state ) return oslate_record();
           return prev_state->lookup<slate_record>( id );
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
           const chain_interface_ptr prev_state = _prev_state.lock();
           if( !prev_state ) return obalance_record();
           return prev_state->lookup<balance_record>( id );
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
           const chain_interface_ptr prev_state = _prev_state.lock();
           if( !prev_state ) return otransaction_record();
           return prev_state->lookup<transaction_record>( id );
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
           const chain_interface_ptr prev_state = _prev_state.lock();
           if( !prev_state ) return ofeed_record();
           return prev_state->lookup<feed_record>( index );
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
           const chain_interface_ptr prev_state = _prev_state.lock();
           if( !prev_state ) return oslot_record();
           return prev_state->lookup<slot_record>( index );
       };

       interface.lookup_by_timestamp = [ this ]( const time_point_sec timestamp ) -> oslot_record
       {
           const auto iter = _slot_timestamp_to_delegate.find( timestamp );
           if( iter != _slot_timestamp_to_delegate.end() ) return _slot_index_to_record.at( slot_index( iter->second, timestamp ) );
           const chain_interface_ptr prev_state = _prev_state.lock();
           if( !prev_state ) return oslot_record();
           const oslot_record record = prev_state->lookup<slot_record>( timestamp );
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
