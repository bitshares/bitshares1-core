#include <bts/blockchain/pending_chain_state.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/log/logger.hpp>

namespace bts { namespace blockchain {
   pending_chain_state::pending_chain_state( chain_interface_ptr prev_state )
   :_prev_state( prev_state )
   {
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
   /** apply changes from this pending state to the previous state */
   void  pending_chain_state::apply_changes()const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      if( !prev_state ) return;
      for( auto item   : properties )     prev_state->set_property( (chain_property_enum)item.first, item.second );
      for( auto record : assets )         prev_state->store_asset_record( record.second );
      for( auto record : accounts )       prev_state->store_account_record( record.second );
      for( auto record : balances )       prev_state->store_balance_record( record.second );
      for( auto record : proposals )      prev_state->store_proposal_record( record.second );
      for( auto record : proposal_votes ) prev_state->store_proposal_vote( record.second );
      for( auto record : bids )           prev_state->store_bid_record( record.first, record.second );
      for( auto record : asks )           prev_state->store_ask_record( record.first, record.second );
      for( auto record : shorts )         prev_state->store_short_record( record.first, record.second );
      for( auto record : collateral )     prev_state->store_collateral_record( record.first, record.second );
      for( auto record : transactions )   prev_state->store_transaction( record.first, record.second );
   }

   otransaction_record  pending_chain_state::get_transaction( const transaction_id_type& trx_id, 
                                                              bool exact  )const
   {
      auto itr = transactions.find( trx_id );
      if( itr != transactions.end() ) return itr->second;
      chain_interface_ptr prev_state = _prev_state.lock();
      return prev_state->get_transaction( trx_id, exact );
   }

   void pending_chain_state::store_transaction( const transaction_id_type& id,
                                                const transaction_record& rec )
   {
      transactions[id] = rec;
   }

   void  pending_chain_state::get_undo_state( const chain_interface_ptr& undo_state_arg )const
   {
      auto undo_state = std::dynamic_pointer_cast<pending_chain_state>(undo_state_arg);
      chain_interface_ptr prev_state = _prev_state.lock();
      FC_ASSERT( prev_state );
      for( auto item : properties )
      {
         auto previous_value = prev_state->get_property( (chain_property_enum)item.first );
         undo_state->set_property( (chain_property_enum)item.first, previous_value );
      }

      for( auto record : assets )
      {
         auto prev_asset = prev_state->get_asset_record( record.first );
         if( !!prev_asset ) undo_state->store_asset_record( *prev_asset );
         else undo_state->store_asset_record( record.second.make_null() );
      }

      for( auto record : accounts )
      {
         auto prev_name = prev_state->get_account_record( record.first );
         if( !!prev_name ) undo_state->store_account_record( *prev_name );
         else undo_state->store_account_record( record.second.make_null() );
      }

      for( auto record : proposals )
      {
         auto prev_proposal = prev_state->get_proposal_record( record.first );
         if( !!prev_proposal ) undo_state->store_proposal_record( *prev_proposal );
         else undo_state->store_proposal_record( record.second.make_null() );
      }
      for( auto record : proposal_votes )
      {
         auto prev_proposal_vote = prev_state->get_proposal_vote( record.first );
         if( !!prev_proposal_vote ) undo_state->store_proposal_vote( *prev_proposal_vote );
         else { undo_state->store_proposal_vote( record.second.make_null() ); }
      }

      for( auto record : balances ) 
      {
         auto prev_address = prev_state->get_balance_record( record.first );
         if( !!prev_address ) undo_state->store_balance_record( *prev_address );
         else undo_state->store_balance_record( record.second.make_null() );
      }

      for( auto record : transactions ) 
      {
         auto prev_trx = prev_state->get_transaction( record.first );
         if( !!prev_trx ) undo_state->store_transaction( record.first, *prev_trx );
         else undo_state->store_transaction( record.first, transaction_record() );
      }

      for( auto record : bids )
      {
         auto prev_value = prev_state->get_bid_record( record.first );
         if( prev_value.valid() ) undo_state->store_bid_record( record.first, *prev_value );
         else  undo_state->store_bid_record( record.first, order_record() );
      }
      for( auto record : asks )
      {
         auto prev_value = prev_state->get_ask_record( record.first );
         if( prev_value.valid() ) undo_state->store_ask_record( record.first, *prev_value );
         else  undo_state->store_ask_record( record.first, order_record() );
      }
      for( auto record : shorts )
      {
         auto prev_value = prev_state->get_short_record( record.first );
         if( prev_value.valid() ) undo_state->store_short_record( record.first, *prev_value );
         else  undo_state->store_short_record( record.first, order_record() );
      }
      for( auto record : collateral )
      {
         auto prev_value = prev_state->get_collateral_record( record.first );
         if( prev_value.valid() ) undo_state->store_collateral_record( record.first, *prev_value );
         else  undo_state->store_collateral_record( record.first, collateral_record() );
      }
   }

   /** load the state from a variant */
   void                    pending_chain_state::from_variant( const fc::variant& v )
   {
      fc::from_variant( v, *this );
   }

   /** convert the state to a variant */
   fc::variant                 pending_chain_state::to_variant()const
   {
      fc::variant v;
      fc::to_variant( *this, v );
      return v;
   }


   oasset_record        pending_chain_state::get_asset_record( asset_id_type asset_id )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto itr = assets.find( asset_id );
      if( itr != assets.end() ) 
        return itr->second;
      else if( prev_state ) 
        return prev_state->get_asset_record( asset_id );
      return oasset_record();
   }

   oasset_record         pending_chain_state::get_asset_record( const std::string& symbol )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto itr = symbol_id_index.find( symbol );
      if( itr != symbol_id_index.end() ) 
        return get_asset_record( itr->second );
      else if( prev_state ) 
        return prev_state->get_asset_record( symbol );
      return oasset_record();
   }
   int64_t  pending_chain_state::get_fee_rate()const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      if( prev_state ) 
        return prev_state->get_fee_rate();
      FC_ASSERT( false, "No current fee rate set" );
   }
   int64_t  pending_chain_state::get_delegate_pay_rate()const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      if( prev_state ) 
        return prev_state->get_delegate_pay_rate();
      FC_ASSERT( false, "No current delegate_pay rate set" );
   }

   fc::time_point_sec  pending_chain_state::now()const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      if( prev_state ) 
        return prev_state->now();
      FC_ASSERT( false, "No current timestamp set" );
   }

   obalance_record      pending_chain_state::get_balance_record( const balance_id_type& balance_id )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto itr = balances.find( balance_id );
      if( itr != balances.end() ) 
        return itr->second;
      else if( prev_state ) 
        return prev_state->get_balance_record( balance_id );
      return obalance_record();
   }

   oaccount_record         pending_chain_state::get_account_record( const address& owner )const
   {
      auto itr = key_to_account.find(owner);
      if( itr != key_to_account.end() ) return get_account_record( itr->second );
      chain_interface_ptr prev_state = _prev_state.lock();
      FC_ASSERT(prev_state);
      return prev_state->get_account_record( owner );
   }


   oaccount_record         pending_chain_state::get_account_record( account_id_type account_id )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto itr = accounts.find( account_id );
      if( itr != accounts.end() ) 
        return itr->second;
      else if( prev_state ) 
        return prev_state->get_account_record( account_id );
      return oaccount_record();
   }

   oaccount_record         pending_chain_state::get_account_record( const std::string& name )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto itr = account_id_index.find( name );
      if( itr != account_id_index.end() ) 
        return get_account_record( itr->second );
      else if( prev_state ) 
        return prev_state->get_account_record( name );
      return oaccount_record();
   }

   void        pending_chain_state::store_asset_record( const asset_record& r )
   {
      assets[r.id] = r;
   }

   void      pending_chain_state::store_balance_record( const balance_record& r )
   {
      balances[r.id()] = r;
   }

   void         pending_chain_state::store_account_record( const account_record& r )
   {
      accounts[r.id] = r;
      account_id_index[r.name] = r.id;
      for( auto item : r.active_key_history )
         key_to_account[address(item.second)] = r.id;
      key_to_account[address(r.owner_key)] = r.id;
   }

   fc::variant  pending_chain_state::get_property( chain_property_enum property_id )const
   {
      auto property_itr = properties.find( property_id );
      if( property_itr != properties.end()  ) return property_itr->second;
      chain_interface_ptr prev_state = _prev_state.lock();
      if( prev_state ) return prev_state->get_property( property_id );
      return fc::variant();
   }
   void  pending_chain_state::set_property( chain_property_enum property_id, 
                                                     const fc::variant& property_value )
   {
      properties[property_id] = property_value;
   }

   void                  pending_chain_state::store_proposal_record( const proposal_record& r )
   {
      proposals[r.id] = r;
   }

   oproposal_record      pending_chain_state::get_proposal_record( proposal_id_type id )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto rec_itr = proposals.find(id);
      if( rec_itr != proposals.end() ) return rec_itr->second;
      else if( prev_state ) return prev_state->get_proposal_record( id );
      return oproposal_record();
   }
                                                                                                          
   void                  pending_chain_state::store_proposal_vote( const proposal_vote& r )
   {
      proposal_votes[r.id] = r;
   }

   oproposal_vote        pending_chain_state::get_proposal_vote( proposal_vote_id_type id )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto rec_itr = proposal_votes.find(id);
      if( rec_itr != proposal_votes.end() ) return rec_itr->second;
      else if( prev_state ) return prev_state->get_proposal_vote( id );
      return oproposal_vote();
   }
   oorder_record         pending_chain_state::get_bid_record( const market_index_key& key )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto rec_itr = bids.find( key );
      if( rec_itr != bids.end() ) return rec_itr->second;
      else if( prev_state ) return prev_state->get_bid_record( key );
      return oorder_record();
   }
   oorder_record         pending_chain_state::get_ask_record( const market_index_key& key )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto rec_itr = asks.find( key );
      if( rec_itr != asks.end() ) return rec_itr->second;
      else if( prev_state ) return prev_state->get_ask_record( key );
      return oorder_record();
   }
   oorder_record         pending_chain_state::get_short_record( const market_index_key& key )const
   {
      chain_interface_ptr prev_state = _prev_state.lock();
      auto rec_itr = shorts.find( key );
      if( rec_itr == shorts.end() ) return rec_itr->second;
      else if( prev_state ) return prev_state->get_short_record( key );
      return oorder_record();
   }
   ocollateral_record    pending_chain_state::get_collateral_record( const market_index_key& key )const
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


} } // bts::blockchain
