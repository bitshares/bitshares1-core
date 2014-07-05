#include <bts/blockchain/pending_chain_state.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/log/logger.hpp>

namespace bts { namespace blockchain {
   pending_chain_state::pending_chain_state( chain_interface_ptr prev_state )
   :_prev_state( prev_state )
   {
   }

   uint32_t pending_chain_state::get_head_block_num() const
   {
        auto state = _prev_state.lock();
        return state->get_head_block_num();
   }

   fc::ripemd160  pending_chain_state::get_current_random_seed()const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      if( prev_state ) 
         return prev_state->get_current_random_seed();
      return fc::ripemd160();
   }

   void pending_chain_state::set_prev_state( chain_interface_ptr prev_state )
   {
      _prev_state = prev_state;
   }

   pending_chain_state::~pending_chain_state(){}

   /**
    *  Based upon the current state of the database, calculate any updates that
    *  should be executed in a deterministic manner.
    */
   void pending_chain_state::apply_deterministic_updates()
   {
      /** nothing to do for now... charge 5% inactivity fee? */
      /** execute order matching */
   }

   /** polymorphically allcoate a new state */
   chain_interface_ptr pending_chain_state::create( const chain_interface_ptr& prev_state )const
   {
      return std::make_shared<pending_chain_state>(prev_state);
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
      for( const auto& item : proposals )       prev_state->store_proposal_record( item.second );
      for( const auto& item : proposal_votes )  prev_state->store_proposal_vote( item.second );
      for( const auto& item : bids )            prev_state->store_bid_record( item.first, item.second );
      for( const auto& item : asks )            prev_state->store_ask_record( item.first, item.second );
      for( const auto& item : shorts )          prev_state->store_short_record( item.first, item.second );
      for( const auto& item : collateral )      prev_state->store_collateral_record( item.first, item.second );
      for( const auto& item : transactions )    prev_state->store_transaction( item.first, item.second );
      for( const auto& item : slates )          prev_state->store_delegate_slate( item.first, item.second );
      for( const auto& item : _block_stats )    prev_state->store_delegate_block_stats( item.first.first, item.first.second, item.second );
      
      for( const auto& item : domains )         prev_state->store_domain_record( item.second );
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
   }

   void pending_chain_state::get_undo_state( const chain_interface_ptr& undo_state_arg )const
   {
      auto undo_state = std::dynamic_pointer_cast<pending_chain_state>(undo_state_arg);
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
      for( const auto& item : proposals )
      {
         auto prev_value = prev_state->get_proposal_record( item.first );
         if( !!prev_value ) undo_state->store_proposal_record( *prev_value );
         else undo_state->store_proposal_record( item.second.make_null() );
      }
      for( const auto& item : proposal_votes )
      {
         auto prev_value = prev_state->get_proposal_vote( item.first );
         if( !!prev_value ) undo_state->store_proposal_vote( *prev_value );
         else { undo_state->store_proposal_vote( item.second.make_null() ); }
      }
      for( const auto& item : balances ) 
      {
         auto prev_value = prev_state->get_balance_record( item.first );
         if( !!prev_value ) undo_state->store_balance_record( *prev_value );
         else undo_state->store_balance_record( item.second.make_null() );
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
      for( const auto& item : domains )
      {
         auto prev_value = prev_state->get_domain_record( item.first );
         if( prev_value.valid() ) undo_state->store_domain_record( *prev_value );
         else  undo_state->store_domain_record( domain_record() );
      }
      for( const auto& item : _block_stats )
      {
         auto prev_value = prev_state->get_delegate_block_stats( item.first.first, item.first.second );
         if( prev_value ) undo_state->store_delegate_block_stats( item.first.first, item.first.second, *prev_value );
         else
         {
             delegate_block_stats invalid_block_stats;
             invalid_block_stats.missed = false;
             invalid_block_stats.id = block_id_type();
             undo_state->store_delegate_block_stats( item.first.first, item.first.second, invalid_block_stats );
         }
      }
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

   oasset_record pending_chain_state::get_asset_record( asset_id_type asset_id )const
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

   share_type pending_chain_state::get_fee_rate()const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      if( prev_state ) 
        return prev_state->get_fee_rate();
      FC_ASSERT( false, "No current fee rate set" );
   }

   share_type pending_chain_state::get_delegate_pay_rate()const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      if( prev_state ) 
        return prev_state->get_delegate_pay_rate();
      FC_ASSERT( false, "No current delegate_pay rate set" );
   }

   fc::time_point_sec pending_chain_state::now()const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      if( prev_state ) 
        return prev_state->now();
      FC_ASSERT( false, "No current timestamp set" );
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


    // DNS
   odomain_record pending_chain_state::get_domain_record( const std::string& domain_name )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto itr = domains.find( domain_name );
      if( itr != domains.end() ) 
        itr->second;
      else if( prev_state ) 
        return prev_state->get_domain_record( domain_name );
      return odomain_record();
   }

   void pending_chain_state::store_domain_record( const domain_record& r )
   {
      domains[r.domain_name] = r;
   }

   vector<domain_record>   pending_chain_state::get_domain_records( const string& first_name,
                                                                uint32_t count )const
   {
        FC_ASSERT(!"unimplemented");
   }
   vector<domain_record>   pending_chain_state::get_domains_in_auction()const
   {
        FC_ASSERT(!"unimplemented");
   }


    // END DNS






   oaccount_record pending_chain_state::get_account_record( account_id_type account_id )const
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
      balances[r.id()] = r;
   }

   void pending_chain_state::store_account_record( const account_record& r )
   {
      accounts[r.id] = r;
      account_id_index[r.name] = r.id;
      for( const auto& item : r.active_key_history )
         key_to_account[address(item.second)] = r.id;
      key_to_account[address(r.owner_key)] = r.id;
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

   void pending_chain_state::store_proposal_record( const proposal_record& r )
   {
      proposals[r.id] = r;
   }

   oproposal_record pending_chain_state::get_proposal_record( proposal_id_type id )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto rec_itr = proposals.find(id);
      if( rec_itr != proposals.end() ) return rec_itr->second;
      else if( prev_state ) return prev_state->get_proposal_record( id );
      return oproposal_record();
   }
                                                                                                          
   void pending_chain_state::store_proposal_vote( const proposal_vote& r )
   {
      proposal_votes[r.id] = r;
   }

   oproposal_vote pending_chain_state::get_proposal_vote( proposal_vote_id_type id )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto rec_itr = proposal_votes.find(id);
      if( rec_itr != proposal_votes.end() ) return rec_itr->second;
      else if( prev_state ) return prev_state->get_proposal_vote( id );
      return oproposal_vote();
   }

   oorder_record pending_chain_state::get_bid_record( const market_index_key& key )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto rec_itr = bids.find( key );
      if( rec_itr != bids.end() ) return rec_itr->second;
      else if( prev_state ) return prev_state->get_bid_record( key );
      return oorder_record();
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
      if( rec_itr == shorts.end() ) return rec_itr->second;
      else if( prev_state ) return prev_state->get_short_record( key );
      return oorder_record();
   }

   ocollateral_record pending_chain_state::get_collateral_record( const market_index_key& key )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto rec_itr = collateral.find( key );
      if( rec_itr == collateral.end() ) return rec_itr->second;
      else if( prev_state ) return prev_state->get_collateral_record( key );
      return ocollateral_record();
   }
                                                                                              
   void pending_chain_state::store_bid_record( const market_index_key& key, const order_record& rec ) 
   {
      bids[key] = rec;
   }

   void pending_chain_state::store_ask_record( const market_index_key& key, const order_record& rec ) 
   {
      asks[key] = rec;
   }
   void pending_chain_state::store_short_record( const market_index_key& key, const order_record& rec )
   {
      shorts[key] = rec;
   }

   void pending_chain_state::store_collateral_record( const market_index_key& key, const collateral_record& rec ) 
   {
      collateral[key] = rec;
   }

   void pending_chain_state::store_delegate_block_stats( const account_id_type& delegate_id, uint32_t block_num,
                                                         const delegate_block_stats& block_stats )
   {
       _block_stats[ std::make_pair( delegate_id, block_num ) ] = block_stats;
   }

   odelegate_block_stats pending_chain_state::get_delegate_block_stats( const account_id_type& delegate_id,
                                                                        uint32_t block_num )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto key = std::make_pair( delegate_id, block_num );
      auto itr = _block_stats.find(key);
      if( itr != _block_stats.end() ) return itr->second;
      if( prev_state ) return prev_state->get_delegate_block_stats( key.first, key.second );
      return odelegate_block_stats();
   }

} } // bts::blockchain
