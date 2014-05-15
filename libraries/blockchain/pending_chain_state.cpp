#include <bts/blockchain/pending_chain_state.hpp>
#include <fc/reflect/variant.hpp>

namespace bts { namespace blockchain {
   pending_chain_state::pending_chain_state( chain_interface_ptr prev_state )
   :new_asset_ids(0),new_name_ids(0),_prev_state( prev_state )
   {
   }


   pending_chain_state::~pending_chain_state()
   {
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

   /** polymorphically allcoate a new state */
   chain_interface_ptr pending_chain_state::create( const chain_interface_ptr& prev_state )const
   {
      return std::make_shared<pending_chain_state>(prev_state);
   }
   /** apply changes from this pending state to the previous state */
   void  pending_chain_state::apply_changes()const
   {
      if( !_prev_state ) return;
      for( auto record : assets )    _prev_state->store_asset_record( record.second );
      for( auto record : names )     _prev_state->store_name_record( record.second );
      for( auto record : accounts ) _prev_state->store_account_record( record.second );
      for( auto record : unique_transactions )
         _prev_state->store_transaction_location( record.first, record.second );
   }

   void  pending_chain_state::get_undo_state( const chain_interface_ptr& undo_state_arg )const
   {
      auto undo_state = std::dynamic_pointer_cast<pending_chain_state>(undo_state_arg);
      FC_ASSERT( _prev_state );
      undo_state->new_asset_ids   = new_asset_ids;
      undo_state->new_name_ids    = new_name_ids;

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

      for( auto record : accounts )
      {
         auto prev_address = _prev_state->get_account_record( record.first );
         if( !!prev_address ) undo_state->store_account_record( *prev_address );
         else undo_state->new_accounts.push_back( record.first );
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
      if( tran_itr != unique_transactions.end() ) return tran_itr->second;
      if( _prev_state ) return _prev_state->get_transaction_location( trx_id );
      return loc;
   }

   oasset_record        pending_chain_state::get_asset_record( asset_id_type asset_id )const
   {
       auto itr = assets.find( asset_id );
       if( itr != assets.end() ) return itr->second;
       else if( _prev_state ) return _prev_state->get_asset_record( asset_id );
       return oasset_record();
   }

   oasset_record         pending_chain_state::get_asset_record( const std::string& symbol )const
   {
      auto itr = symbol_id_index.find( symbol );
      if( itr != symbol_id_index.end() ) return get_asset_record( itr->second );
      else if( _prev_state ) return _prev_state->get_asset_record( symbol );
      return oasset_record();
   }
   int64_t  pending_chain_state::get_fee_rate()const
   {
      if( _prev_state ) return _prev_state->get_fee_rate();
      FC_ASSERT( !"No current fee rate set" );
   }
   int64_t  pending_chain_state::get_delegate_pay_rate()const
   {
      if( _prev_state ) return _prev_state->get_delegate_pay_rate();
      FC_ASSERT( !"No current delegate_pay rate set" );
   }

   fc::time_point_sec  pending_chain_state::timestamp()const
   {
      if( _prev_state ) return _prev_state->timestamp();
      FC_ASSERT( !"No current timestamp set" );
   }

   oaccount_record      pending_chain_state::get_account_record( const account_id_type& account_id )const
   {
       auto itr = accounts.find( account_id );
       if( itr != accounts.end() ) return itr->second;
       else if( _prev_state ) return _prev_state->get_account_record( account_id );
       return oaccount_record();
   }

   oname_record         pending_chain_state::get_name_record( name_id_type name_id )const
   {
       auto itr = names.find( name_id );
       if( itr != names.end() ) return itr->second;
       else if( _prev_state ) return _prev_state->get_name_record( name_id );
       return oname_record();
   }

   oname_record         pending_chain_state::get_name_record( const std::string& name )const
   {
      auto itr = name_id_index.find( name );
      if( itr != name_id_index.end() ) return get_name_record( itr->second );
      else if( _prev_state ) return _prev_state->get_name_record( name );
      return oname_record();
   }

   void        pending_chain_state::store_asset_record( const asset_record& r )
   {
      assets[r.id] = r;
   }

   void      pending_chain_state::store_account_record( const account_record& r )
   {
      accounts[r.id()] = r;
   }

   void         pending_chain_state::store_name_record( const name_record& r )
   {
      names[r.id] = r;
      name_id_index[r.name] = r.id;
   }

   asset_id_type pending_chain_state::last_asset_id()const
   {
      if( _prev_state ) return _prev_state->last_asset_id() + new_asset_ids;
      return new_asset_ids;
   }
   name_id_type pending_chain_state::last_name_id()const
   {
      if( _prev_state ) return _prev_state->last_name_id() + new_name_ids;
      return new_name_ids;
   }

   asset_id_type  pending_chain_state::new_asset_id()
   {
      auto last = _prev_state->last_asset_id();
      new_asset_ids++;
      return last + new_asset_ids;
   }

   name_id_type  pending_chain_state::new_name_id()
   {
      auto last = _prev_state->last_name_id();
      new_name_ids++;
      return last + new_name_ids;
   }

} } // bts::blockchain
