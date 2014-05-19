#include <bts/blockchain/pending_chain_state.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/log/logger.hpp>

namespace bts { namespace blockchain {
   pending_chain_state::pending_chain_state( chain_interface_ptr prev_state )
   :_prev_state( prev_state )
   {
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
      if( !_prev_state ) return;
      for( auto item   : properties ) _prev_state->set_property( (chain_property_enum)item.first, item.second );
      for( auto record : assets )    _prev_state->store_asset_record( record.second );
      for( auto record : names )     _prev_state->store_name_record( record.second );
      for( auto record : balances ) _prev_state->store_balance_record( record.second );
      for( auto record : unique_transactions ) 
         _prev_state->store_transaction_location( record.first, record.second );
   }

   void  pending_chain_state::get_undo_state( const chain_interface_ptr& undo_state_arg )const
   {
      auto undo_state = std::dynamic_pointer_cast<pending_chain_state>(undo_state_arg);
      FC_ASSERT( _prev_state );
      for( auto item : properties )
      {
         auto previous_value = _prev_state->get_property( (chain_property_enum)item.first );
         undo_state->set_property( (chain_property_enum)item.first, previous_value );
      }

      for( auto record : assets )
      {
         auto prev_asset = _prev_state->get_asset_record( record.first );
         if( !!prev_asset ) undo_state->store_asset_record( *prev_asset );
      }

      for( auto record : names )
      {
         auto prev_name = _prev_state->get_name_record( record.first );
         if( !!prev_name ) undo_state->store_name_record( *prev_name );
      }

      for( auto record : balances ) 
      {
         auto prev_address = _prev_state->get_balance_record( record.first );
         if( !!prev_address ) undo_state->store_balance_record( *prev_address );
         else
         {
            auto tmp = record.second;
            tmp.balance = 0; // balance of 0 are removed.
            undo_state->store_balance_record( tmp );
         }
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

   void pending_chain_state::store_transaction_location( const transaction_id_type& id,
                                                         const transaction_location& loc )
   {
      unique_transactions[id] = loc;
   }

   otransaction_location pending_chain_state::get_transaction_location( const transaction_id_type& trx_id )const
   {
      otransaction_location loc;
      auto tran_itr = unique_transactions.find( trx_id );
      if( tran_itr != unique_transactions.end() ) 
        return tran_itr->second;
      if( _prev_state ) 
        return _prev_state->get_transaction_location( trx_id );
      return loc;
   }

   oasset_record        pending_chain_state::get_asset_record( asset_id_type asset_id )const
   {
      auto itr = assets.find( asset_id );
      if( itr != assets.end() ) 
        return itr->second;
      else if( _prev_state ) 
        return _prev_state->get_asset_record( asset_id );
      return oasset_record();
   }

   oasset_record         pending_chain_state::get_asset_record( const std::string& symbol )const
   {
      auto itr = symbol_id_index.find( symbol );
      if( itr != symbol_id_index.end() ) 
        return get_asset_record( itr->second );
      else if( _prev_state ) 
        return _prev_state->get_asset_record( symbol );
      return oasset_record();
   }
   int64_t  pending_chain_state::get_fee_rate()const
   {
      if( _prev_state ) 
        return _prev_state->get_fee_rate();
      FC_ASSERT( false, "No current fee rate set" );
   }
   int64_t  pending_chain_state::get_delegate_pay_rate()const
   {
      if( _prev_state ) 
        return _prev_state->get_delegate_pay_rate();
      FC_ASSERT( false, "No current delegate_pay rate set" );
   }

   fc::time_point_sec  pending_chain_state::now()const
   {
      if( _prev_state ) 
        return _prev_state->now();
      FC_ASSERT( false, "No current timestamp set" );
   }

   obalance_record      pending_chain_state::get_balance_record( const balance_id_type& balance_id )const
   {
      auto itr = balances.find( balance_id );
      if( itr != balances.end() ) 
        return itr->second;
      else if( _prev_state ) 
        return _prev_state->get_balance_record( balance_id );
      return obalance_record();
   }

   oname_record         pending_chain_state::get_name_record( name_id_type name_id )const
   {
      auto itr = names.find( name_id );
      if( itr != names.end() ) 
        return itr->second;
      else if( _prev_state ) 
        return _prev_state->get_name_record( name_id );
      return oname_record();
   }

   oname_record         pending_chain_state::get_name_record( const std::string& name )const
   {
      auto itr = name_id_index.find( name );
      if( itr != name_id_index.end() ) 
        return get_name_record( itr->second );
      else if( _prev_state ) 
        return _prev_state->get_name_record( name );
      return oname_record();
   }

   void        pending_chain_state::store_asset_record( const asset_record& r )
   {
      assets[r.id] = r;
   }

   void      pending_chain_state::store_balance_record( const balance_record& r )
   {
      balances[r.id()] = r;
   }

   void         pending_chain_state::store_name_record( const name_record& r )
   {
      names[r.id] = r;
      name_id_index[r.name] = r.id;
   }

   fc::variant  pending_chain_state::get_property( chain_property_enum property_id )const
   {
      auto property_itr = properties.find( property_id );
      if( property_itr != properties.end()  ) return property_itr->second;
      if( _prev_state ) return _prev_state->get_property( property_id );
      return fc::variant();
   }
   void  pending_chain_state::set_property( chain_property_enum property_id, 
                                                     const fc::variant& property_value )
   {
      properties[property_id] = property_value;
   }

} } // bts::blockchain
