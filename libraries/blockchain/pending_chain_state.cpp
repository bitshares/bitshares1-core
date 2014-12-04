#include <bts/blockchain/pending_chain_state.hpp>

namespace bts { namespace blockchain {

   pending_chain_state::pending_chain_state( chain_interface_ptr prev_state )
   :_prev_state( prev_state )
   {
   }

   pending_chain_state::~pending_chain_state()
   {
   }

   /** polymorphically allcoate a new state */
   chain_interface_ptr pending_chain_state::create( const chain_interface_ptr& prev_state )const
   {
      return std::make_shared<pending_chain_state>( prev_state );
   }

   void pending_chain_state::set_prev_state( chain_interface_ptr prev_state )
   {
      _prev_state = prev_state;
   }

   uint32_t pending_chain_state::get_head_block_num()const
   {
      const chain_interface_ptr prev_state = _prev_state.lock();
      FC_ASSERT( prev_state );
      return prev_state->get_head_block_num();
   }

   fc::time_point_sec pending_chain_state::now()const
   {
      const chain_interface_ptr prev_state = _prev_state.lock();
      FC_ASSERT( prev_state );
      return prev_state->now();
   }

   fc::ripemd160 pending_chain_state::get_current_random_seed()const
   {
      const chain_interface_ptr prev_state = _prev_state.lock();
      FC_ASSERT( prev_state );
      return prev_state->get_current_random_seed();
   }

   /**
    *  Based upon the current state of the database, calculate any updates that
    *  should be executed in a deterministic manner.
    */
   void pending_chain_state::apply_deterministic_updates()
   {
      /** nothing to do for now... charge 5% inactivity fee? */
      /** execute order matching */
   }

   /** Apply changes from this pending state to the previous state */
   void pending_chain_state::apply_changes()const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      if( !prev_state ) return;
      for( const auto& item : properties )      prev_state->set_property( (chain_property_enum)item.first, item.second );
      for( const auto& item : assets )          prev_state->store_asset_record( item.second );
      for( const auto& item : accounts )        prev_state->store_account_record( item.second );
      for( const auto& item : balances )        prev_state->store_balance_record( item.second );
      for( const auto& item : authorizations )  prev_state->authorize( item.first.first, item.first.second, item.second );
      for( const auto& item : bids )            prev_state->store_bid_record( item.first, item.second );
      for( const auto& item : relative_bids )   prev_state->store_relative_bid_record( item.first, item.second );
      for( const auto& item : asks )            prev_state->store_ask_record( item.first, item.second );
      for( const auto& item : relative_asks )   prev_state->store_relative_ask_record( item.first, item.second );
      for( const auto& item : shorts )          prev_state->store_short_record( item.first, item.second );
      for( const auto& item : collateral )      prev_state->store_collateral_record( item.first, item.second );
      for( const auto& item : transactions )    prev_state->store_transaction( item.first, item.second );
      for( const auto& item : slates )          prev_state->store_delegate_slate( item.first, item.second );
      for( const auto& item : slots )           prev_state->store_slot_record( item.second );
      for( const auto& item : market_history )  prev_state->store_market_history_record( item.first, item.second );
      for( const auto& item : market_statuses ) prev_state->store_market_status( item.second );
      for( const auto& item : asset_proposals ) prev_state->store_asset_proposal( item.second );
      for( const auto& item : feeds )           prev_state->set_feed( item.second );
      for( const auto& items : recent_operations )
      {
         for( const auto& item : items.second )    prev_state->store_recent_operation( item );
      }
      for( const auto& item : burns ) prev_state->store_burn_record( burn_record(item.first,item.second) );
      for( const auto& item : objects ) prev_state->store_object_record( item.second );
      prev_state->set_market_transactions( market_transactions );
      prev_state->set_dirty_markets( _dirty_markets );
   }

   otransaction_record pending_chain_state::get_transaction( const transaction_id_type& trx_id,
                                                              bool exact  )const
   {
      auto itr = transactions.find( trx_id );
      if( itr != transactions.end() ) return itr->second;
      chain_interface_ptr prev_state = _prev_state.lock();
      return prev_state->get_transaction( trx_id, exact );
   }

   bool pending_chain_state::is_known_transaction( const transaction_id_type& id )
   { try {
      auto itr = transactions.find( id );
      if( itr != transactions.end() ) return true;
      chain_interface_ptr prev_state = _prev_state.lock();
      return prev_state->is_known_transaction( id );
   } FC_CAPTURE_AND_RETHROW( (id) ) }

   void pending_chain_state::store_transaction( const transaction_id_type& id,
                                                const transaction_record& rec )
   {
      transactions[id] = rec;

      for( const auto& op : rec.trx.operations )
        store_recent_operation(op);
   }

   void pending_chain_state::get_undo_state( const chain_interface_ptr& undo_state_arg )const
   {
      auto undo_state = std::dynamic_pointer_cast<pending_chain_state>( undo_state_arg );
      chain_interface_ptr prev_state = _prev_state.lock();
      FC_ASSERT( prev_state );
      for( const auto& item : properties )
      {
         auto prev_value = prev_state->get_property( (chain_property_enum)item.first );
         undo_state->set_property( (chain_property_enum)item.first, prev_value );
      }
      for( const auto& item : assets )
      {
         auto prev_value = prev_state->get_asset_record( item.first );
         if( !!prev_value ) undo_state->store_asset_record( *prev_value );
         else undo_state->store_asset_record( item.second.make_null() );
      }
      for( const auto& item : slates )
      {
         auto prev_value = prev_state->get_delegate_slate( item.first );
         if( prev_value ) undo_state->store_delegate_slate( item.first, *prev_value );
         else undo_state->store_delegate_slate( item.first, delegate_slate() );
      }
      for( const auto& item : accounts )
      {
         auto prev_value = prev_state->get_account_record( item.first );
         if( !!prev_value ) undo_state->store_account_record( *prev_value );
         else undo_state->store_account_record( item.second.make_null() );
      }
      for( const auto& item : asset_proposals )
      {
         auto prev_value = prev_state->fetch_asset_proposal( item.first.first, item.first.second );
         if( !!prev_value ) undo_state->store_asset_proposal( *prev_value );
         else undo_state->store_asset_proposal( item.second.make_null() );
      }
      for( const auto& item : balances )
      {
         auto prev_value = prev_state->get_balance_record( item.first );
         if( !!prev_value ) undo_state->store_balance_record( *prev_value );
         else undo_state->store_balance_record( item.second.make_null() );
      }
      for( const auto& item : authorizations )
      {
         auto prev_value = prev_state->get_authorization( item.first.first, item.first.second );
         if( !!prev_value ) undo_state->authorize( item.first.first, item.first.second, *prev_value );
         else undo_state->deauthorize( item.first.first, item.first.second );
      }
      for( const auto& item : transactions )
      {
         auto prev_value = prev_state->get_transaction( item.first );
         if( !!prev_value ) undo_state->store_transaction( item.first, *prev_value );
         else undo_state->store_transaction( item.first, transaction_record() );
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
      for( const auto& item : slots )
      {
         auto prev_value = prev_state->get_slot_record( item.first );
         if( prev_value ) undo_state->store_slot_record( *prev_value );
         else
         {
             slot_record invalid_slot_record;
             undo_state->store_slot_record( invalid_slot_record );
         }
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
      for( const auto& item : feeds )
      {
         auto prev_value = prev_state->get_feed( item.first );
         if( prev_value ) undo_state->set_feed( *prev_value );
         else undo_state->set_feed( feed_record{item.first} );
      }
      for( const auto& item : burns )
      {
         undo_state->store_burn_record( burn_record( item.first ) );
      }
      for( const auto& item : objects )
      {
         undo_state->store_object_record( object_record( item.first ) );
      }

      const auto dirty_markets = prev_state->get_dirty_markets();
      undo_state->set_dirty_markets( dirty_markets );

      /* NOTE: Recent operations are currently not rewound on undo */
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

   oasset_record pending_chain_state::get_asset_record( const asset_id_type& asset_id )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto itr = assets.find( asset_id );
      if( itr != assets.end() )
        return itr->second;
      else if( prev_state )
        return prev_state->get_asset_record( asset_id );
      return oasset_record();
   }

   oasset_record pending_chain_state::get_asset_record( const std::string& symbol )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto itr = symbol_id_index.find( symbol );
      if( itr != symbol_id_index.end() )
        return get_asset_record( itr->second );
      else if( prev_state )
        return prev_state->get_asset_record( symbol );
      return oasset_record();
   }

   obalance_record pending_chain_state::get_balance_record( const balance_id_type& balance_id )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto itr = balances.find( balance_id );
      if( itr != balances.end() )
        return itr->second;
      else if( prev_state )
        return prev_state->get_balance_record( balance_id );
      return obalance_record();
   }

   odelegate_slate pending_chain_state::get_delegate_slate( slate_id_type id )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto itr = slates.find(id);
      if( itr != slates.end() ) return itr->second;
      if( prev_state ) return prev_state->get_delegate_slate( id );
      return odelegate_slate();
   }

   void pending_chain_state::store_delegate_slate( slate_id_type id, const delegate_slate& slate )
   {
      slates[id] = slate;
   }

   oaccount_record pending_chain_state::get_account_record( const address& owner )const
   {
      auto itr = key_to_account.find(owner);
      if( itr != key_to_account.end() ) return get_account_record( itr->second );
      chain_interface_ptr prev_state = _prev_state.lock();
      FC_ASSERT(prev_state);
      return prev_state->get_account_record( owner );
   }

   oaccount_record pending_chain_state::get_account_record( const account_id_type& account_id )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto itr = accounts.find( account_id );
      if( itr != accounts.end() )
        return itr->second;
      else if( prev_state )
        return prev_state->get_account_record( account_id );
      return oaccount_record();
   }

   oaccount_record pending_chain_state::get_account_record( const std::string& name )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto itr = account_id_index.find( name );
      if( itr != account_id_index.end() )
        return get_account_record( itr->second );
      else if( prev_state )
        return prev_state->get_account_record( name );
      return oaccount_record();
   }

   void pending_chain_state::store_asset_record( const asset_record& r )
   {
      assets[r.id] = r;
   }

   void pending_chain_state::store_balance_record( const balance_record& r )
   {
       ilog("calling store_balance_record: ${r}", ("r", r));
      balances[r.id()] = r;
   }

   void pending_chain_state::store_account_record( const account_record& r )
   {
      accounts[ r.id ] = r;
      account_id_index[ r.name ] = r.id;
      key_to_account[ r.owner_address() ] = r.id;
      if( !r.is_retracted() )
          key_to_account[ r.active_address() ] = r.id;
      if( r.is_delegate() )
          key_to_account[ r.signing_address() ] = r.id;
   }

   vector<operation> pending_chain_state::get_recent_operations(operation_type_enum t)
   {
      const auto& recent_op_queue = recent_operations[t];
      vector<operation> recent_ops(recent_op_queue.size());
      std::copy(recent_op_queue.begin(), recent_op_queue.end(), recent_ops.begin());
      return recent_ops;
   }

   void pending_chain_state::store_recent_operation(const operation& o)
   {
      auto& recent_op_queue = recent_operations[o.type];
      recent_op_queue.push_back(o);
      if( recent_op_queue.size() > MAX_RECENT_OPERATIONS )
        recent_op_queue.pop_front();
   }

   oobject_record pending_chain_state::get_object_record(object_id_type id)
   {
       if( objects.find(id) != objects.end() )
           return oobject_record(objects[id]);
       return oobject_record();
   }

   void pending_chain_state::store_object_record(const object_record& obj)
   {
       objects[obj._id] = obj;
   }


   fc::variant pending_chain_state::get_property( chain_property_enum property_id )const
   {
      auto property_itr = properties.find( property_id );
      if( property_itr != properties.end()  ) return property_itr->second;
      chain_interface_ptr prev_state = _prev_state.lock();
      if( prev_state ) return prev_state->get_property( property_id );
      return fc::variant();
   }

   void pending_chain_state::set_property( chain_property_enum property_id,
                                                     const fc::variant& property_value )
   {
      properties[property_id] = property_value;
   }

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

   omarket_order pending_chain_state::get_lowest_ask_record( const asset_id_type& quote_id, const asset_id_type& base_id )
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

   void pending_chain_state::set_market_dirty( const asset_id_type& quote_id, const asset_id_type& base_id )
   {
      _dirty_markets.insert( std::make_pair( quote_id, base_id ) );
   }

   void pending_chain_state::store_collateral_record( const market_index_key& key, const collateral_record& rec )
   {
      collateral[ key ] = rec;
      _dirty_markets.insert( key.order_price.asset_pair() );
   }

   void pending_chain_state::store_slot_record( const slot_record& r )
   {
      slots[ r.start_time ] = r;
   }

   oslot_record pending_chain_state::get_slot_record( const time_point_sec& start_time )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto itr = slots.find( start_time );
      if( itr != slots.end() ) return itr->second;
      if( prev_state ) return prev_state->get_slot_record( start_time );
      return oslot_record();
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

   omarket_status pending_chain_state::get_market_status( const asset_id_type& quote_id, const asset_id_type& base_id )
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

   void pending_chain_state::set_feed( const feed_record& r )
   {
      feeds[r.feed] = r;
   }

   ofeed_record pending_chain_state::get_feed( const feed_index& i )const
   {
      auto itr = feeds.find(i);
      if( itr != feeds.end() ) return itr->second;

      chain_interface_ptr prev_state = _prev_state.lock();
      return prev_state->get_feed(i);
   }

   oprice pending_chain_state::get_median_delegate_price( const asset_id_type& quote_id, const asset_id_type& base_id )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      return prev_state->get_median_delegate_price( quote_id, base_id );
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

   void  pending_chain_state::authorize( asset_id_type asset_id, const address& owner, object_id_type oid  ) 
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      authorizations[std::make_pair(asset_id,owner)] = oid;
   }

   optional<object_id_type>  pending_chain_state::get_authorization( asset_id_type asset_id, const address& owner )const 
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto index = std::make_pair( asset_id, owner );
      auto itr = authorizations.find( index );
      if( itr == authorizations.end() ) return prev_state->get_authorization( asset_id, owner );
      if( itr->second != -1 )
         return itr->second;
      return optional<object_id_type>();
   }
   void                       pending_chain_state::store_asset_proposal( const proposal_record& r ) 
   {
      asset_proposals[std::make_pair( r.asset_id, r.proposal_id )] = r;
   }

   optional<proposal_record>  pending_chain_state::fetch_asset_proposal( asset_id_type asset_id, proposal_id_type proposal_id )const 
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto itr = asset_proposals.find( std::make_pair( asset_id, proposal_id ) );
      if( itr != asset_proposals.end() ) return itr->second;
      return prev_state->fetch_asset_proposal( asset_id, proposal_id );
   }

} } // bts::blockchain
