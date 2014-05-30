#include <bts/wallet/wallet_db.hpp>
#include <bts/db/level_map.hpp>
#include <fc/crypto/aes.hpp>
#include <fc/io/json.hpp>

#include <fc/log/logger.hpp>

namespace bts{ namespace wallet {
   using namespace bts::blockchain;

   namespace detail { 
     class wallet_db_impl
     {
        public:
           wallet_db*                                        self;
           bts::db::level_map<int32_t,generic_wallet_record> _records;

           void load_master_key_record( const wallet_master_key_record& key )
           { try {
              FC_ASSERT( !self->wallet_master_key.valid() );
              self->wallet_master_key = key;
              wlog( "master: ${M}", ("M",key) );
           } FC_RETHROW_EXCEPTIONS( warn, "" ) }

           void load_account_record( const wallet_account_record& account_to_load )
           { try {
              self->accounts[account_to_load.index] = account_to_load;

              auto current_index_itr = self->address_to_account.find( account_to_load.account_address );
              FC_ASSERT( current_index_itr == self->address_to_account.end() );
              self->address_to_account[ account_to_load.account_address ]= account_to_load.index;
              
              if( account_to_load.registered_account_id != 0 )
              {
                auto current_account_id_itr = self->account_id_to_account.find( account_to_load.registered_account_id );
                FC_ASSERT( current_account_id_itr == self->account_id_to_account.end() );
                self->account_id_to_account[ account_to_load.registered_account_id ] = account_to_load.index;
              }

              auto current_name_itr = self->name_to_account.find( account_to_load.name );
              FC_ASSERT( current_name_itr == self->name_to_account.end() );
              self->name_to_account[ account_to_load.name ] = account_to_load.index;

           } FC_RETHROW_EXCEPTIONS( warn, "", ("account_to_load",account_to_load) )  }

           void load_key_record( const wallet_key_record& key_to_load )
           { try {
              address key_address( key_to_load.public_key );
              auto current_key_itr = self->keys.find( key_address );
              FC_ASSERT( current_key_itr == self->keys.end(), "key should be unique" );

              self->keys[key_address] = key_to_load;

              auto key = key_to_load.public_key;
              auto bts_addr = key_to_load.get_address();
              self->btc_to_bts_address[ address(key) ] = bts_addr;
              self->btc_to_bts_address[ address(pts_address(key,false,56) )] = bts_addr;
              self->btc_to_bts_address[ address(pts_address(key,true,56) ) ] = bts_addr;
              self->btc_to_bts_address[ address(pts_address(key,false,0) ) ] = bts_addr;
              self->btc_to_bts_address[ address(pts_address(key,true,0) )  ] = bts_addr;
           } FC_RETHROW_EXCEPTIONS( warn, "", ("key_to_load",key_to_load) ) }

           void load_transaction_record( const wallet_transaction_record& rec )
           { try {
              auto itr = self->transactions.find( rec.trx.id() );
              FC_ASSERT( itr == self->transactions.end(), "Duplicate Transaction found in Wallet" )
              self->transactions[ rec.trx.id() ] = rec;
           } FC_RETHROW_EXCEPTIONS( warn, "", ("rec",rec) ) }

           void load_property_record( const wallet_property_record& property_rec )
           { try {
              auto itr = self->properties.find( property_rec.key );
              FC_ASSERT( itr == self->properties.end(), "Duplicate Property Record" );
              self->properties[property_rec.key] = property_rec;
           } FC_RETHROW_EXCEPTIONS( warn, "", ("property_record",property_rec )) }

           void load_asset_record( const wallet_asset_record& asset_rec )
           { try {
              FC_ASSERT( !"Not Implemented" );
           } FC_RETHROW_EXCEPTIONS( warn, "", ("asset_record",asset_rec )) }


           void load_balance_record( const wallet_balance_record& rec )
           { try {
              // TODO: check for duplicates...
              self->balances[ rec.id() ] = rec;
           } FC_RETHROW_EXCEPTIONS( warn, "", ("rec",rec) ) }
     };

   } // namespace detail


   wallet_db::wallet_db()
   :my( new detail::wallet_db_impl() )
   {
      my->self = this;
   }

   wallet_db::~wallet_db()
   {
   }

   void wallet_db::open( const fc::path& wallet_file )
   { try {
      my->_records.open( wallet_file, true );
      auto current_record_itr = my->_records.begin();
      while( current_record_itr.valid() )
      {
         auto current_record = current_record_itr.value();
         ilog( "load: ${r}", ("r",current_record) );
         try 
         {
            switch( (wallet_record_type_enum)current_record.type )
            {
               case master_key_record_type:
                  my->load_master_key_record( current_record.as<wallet_master_key_record>() );
                  break;
               case account_record_type:
                  my->load_account_record( current_record.as<wallet_account_record>() );
                  break;
               case key_record_type:
                  my->load_key_record( current_record.as<wallet_key_record>() );
                  break;
               case property_record_type:
                  my->load_property_record( current_record.as<wallet_property_record>() );
                  break;
               case balance_record_type:
                  my->load_balance_record( current_record.as<wallet_balance_record>() );
                  break;
               case transaction_record_type:
                  my->load_transaction_record( current_record.as<wallet_transaction_record>() );
                  break;
            }
         } 
         catch ( const fc::exception& e )
         {
            wlog( "Error loading wallet record:\n${r}\nreason: ${e}", ("e",e.to_detail_string())("r",current_record) );
         }
         ++current_record_itr;
      }
   } FC_RETHROW_EXCEPTIONS( warn, "Error opening wallet file ${file}", ("file",wallet_file) ) }

   void wallet_db::close()
   {
      my->_records.close();
      keys.clear();
      wallet_master_key.reset();
      address_to_account.clear();
      account_id_to_account.clear();
      name_to_account.clear();
      accounts.clear();
      transactions.clear();
      balances.clear();
      registered_accounts.clear();
      assets.clear();
      properties.clear();
   }

   bool wallet_db::is_open()const { return my->_records.is_open(); }

   int32_t wallet_db::new_index()
   {
      auto next_rec_num = get_property( next_record_number );
      int32_t next_rec_number = 2;
      if( next_rec_num.is_null() )
      {
         next_rec_number = 2;
      }
      else
      {
         next_rec_number = next_rec_num.as_int64();
      }
      set_property( property_enum::next_record_number, next_rec_number + 1 );
      return next_rec_number;
   }

   int32_t wallet_db::new_key_child_index()
   {
      auto next_child_idx = get_property( next_child_key_index );
      int32_t next_child_index = 0;
      if( next_child_idx.is_null() )
      {
         next_child_index = 1;
      }
      else
      {
         next_child_index = next_child_idx.as_int64();
      }
      set_property( property_enum::next_child_key_index, next_child_index + 1 );
      return next_child_index;
   }

   fc::ecc::private_key wallet_db::new_private_key( const fc::sha512& password, 
                                                    const address& parent_account_address )
   {
      FC_ASSERT( wallet_master_key.valid() );

      auto master_ext_priv_key  = wallet_master_key->decrypt_key( password );
      auto new_priv_key = master_ext_priv_key.child( new_key_child_index() );

      key_data new_key;
      new_key.account_address = parent_account_address;
      new_key.encrypt_private_key( password, new_priv_key );

      store_key( new_key );
      return new_priv_key;
   }

   void wallet_db::store_generic_record( int32_t index, const generic_wallet_record& record )
   { try {
       FC_ASSERT( is_open() );
       FC_ASSERT( index != 0 );
       ilog( "${record}", ("record",record) );
       my->_records.store( index, record );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("record",record) ) }

   void        wallet_db::set_property( property_enum property_id, const variant& v )
   {
      auto property_itr = properties.find( property_id );
      if( property_itr != properties.end() ) property_itr->second.value = v;
      else 
      {
          if( property_id == property_enum::next_record_number )
              properties[property_id] = wallet_property_record( wallet_property(property_id, v), 1 );
          else
              properties[property_id] = wallet_property_record( wallet_property(property_id, v), new_index() );
      }
      store_record( properties[property_id] );
   }

   variant wallet_db::get_property( property_enum property_id )
   {
      auto property_itr = properties.find( property_id );
      if( property_itr != properties.end() ) return property_itr->second.value;
      return variant();
   }

   void wallet_db::store_key( const key_data& key_to_store )
   {
      auto key_itr = keys.find( key_to_store.get_address() );
      if( key_itr != keys.end() )
      {
         key_data& old_data = key_itr->second;
         old_data = key_to_store;
         store_record( key_itr->second );
      }
      else
      {
         auto r = wallet_key_record( key_to_store, new_index() );
         store_record( keys[key_to_store.get_address()] = r );
         
         auto key = key_to_store.public_key;
         auto bts_addr = key_to_store.get_address();
         btc_to_bts_address[ address(key) ] = bts_addr;
         btc_to_bts_address[ address(pts_address(key,false,56) )] = bts_addr;
         btc_to_bts_address[ address(pts_address(key,true,56) ) ] = bts_addr;
         btc_to_bts_address[ address(pts_address(key,false,0) ) ] = bts_addr;
         btc_to_bts_address[ address(pts_address(key,true,0) )  ] = bts_addr;
      }
   }

   void wallet_db::export_to_json( const fc::path& file_name ) const
   { try {
      FC_ASSERT( is_open() );
      std::vector<generic_wallet_record>  records;
      auto itr = my->_records.begin(); 
      while( itr.valid() )
      {
         records.push_back( itr.value() );
         ++itr;
      }
      fc::json::save_to_file( records, file_name, true );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("file_name",file_name) ) }

   void wallet_db::create_from_json( const fc::path& import_file_name, 
                                     const fc::path& wallet_to_create )
   { try {
      FC_ASSERT( !fc::exists( wallet_to_create ) );
      open( wallet_to_create );
      auto input_records = fc::json::from_file<std::vector<generic_wallet_record> >( import_file_name );
      for( auto r : input_records )
      {
         store_generic_record( r.get_index(), r );
      }
   } FC_RETHROW_EXCEPTIONS( warn, "", ("import_file_name",import_file_name)
                                      ("wallet_to_create",wallet_to_create) ) }

   bool wallet_db::has_private_key( const address& a )const
   { try {
      auto itr = keys.find(a);
      if( itr != keys.end() )
         return itr->second.has_private_key();
      return false; 
   } FC_RETHROW_EXCEPTIONS( warn, "", ("address",a) ) }

   void wallet_db::cache_memo( const memo_status& memo, 
                               const fc::ecc::private_key& account_key, 
                               const fc::sha512& password )
   {
      key_data data;
      data.account_address = address(account_key.get_public_key());
      data.memo = memo_data(memo);
      data.valid_from_signature = memo.has_valid_signature;
      data.encrypt_private_key( password, memo.owner_private_key );

      store_key( data );
   }

   private_keys wallet_db::get_account_private_keys( const fc::sha512& password )
   {
       private_keys keys;
       keys.reserve( accounts.size() );
       for( auto item : accounts )
       {
          auto key_rec = lookup_key(item.second.account_address);
          if( key_rec.valid() )
             keys.push_back( key_rec->decrypt_private_key( password ) );
       }
       return keys;
   }
   
   owallet_balance_record wallet_db::lookup_balance( const balance_id_type& balance_id )
   {
      auto itr = balances.find( balance_id );
      if( itr == balances.end() ) return fc::optional<wallet_balance_record>();
      return itr->second;
   }

   owallet_key_record     wallet_db::lookup_key( const address& address )const
   {
      auto btc_to_bts_itr = btc_to_bts_address.find(address);
      if( btc_to_bts_itr != btc_to_bts_address.end() )
      {
         auto itr = keys.find( btc_to_bts_itr->second ); //address );
         if( itr != keys.end() ) return itr->second;
      }
      return owallet_key_record();
   }

   void wallet_db::add_contact_account( const string& new_account_name, 
                                        const public_key_type& new_account_key )
   {
      ilog( "${name}", ("name", new_account_name)  );
      auto current_account_itr = name_to_account.find( new_account_name );
      FC_ASSERT( current_account_itr == name_to_account.end(), 
                 "Account with name ${name} already exists", 
                 ("name",new_account_name) );
      auto current_address_itr = address_to_account.find( new_account_key );
      FC_ASSERT( current_address_itr == address_to_account.end(), 
                 "Account with address ${address} already exists", 
                 ("name",new_account_key) );

      wallet_account_record war; 
      war.name = new_account_name;
      war.registered_account_id = 0;
      war.account_address = address( new_account_key );

      auto current_key = lookup_key( new_account_key );
      if( current_key )
      {  
         wlog( "key exists... ..." );
         current_key->account_address = address(new_account_key);
         store_record( *current_key );
      }
      else
      {
         wlog( "new wallet key exists... ..." );
         wallet_key_record new_key;
         new_key.index = new_index();
         new_key.account_address = address(new_account_key);
         new_key.public_key = new_account_key;
         wlog( "store_key: ${key}", ("key",new_key) );
         my->load_key_record( new_key );
         store_key( new_key );
      }

      war.index = new_index();
      store_record( war );
      my->load_account_record( war );
   }
   owallet_account_record wallet_db::lookup_account( const address& address_of_public_key )
   {
      auto address_index_itr = address_to_account.find( address_of_public_key );
      if( address_index_itr != address_to_account.end() )
      {
         auto account_itr = accounts.find( address_index_itr->second );
         FC_ASSERT( account_itr != accounts.end(),
                    "wallet database in inconsistant state" ); 
         return account_itr->second;
      }
      return owallet_account_record();
   }

   owallet_account_record wallet_db::lookup_account( const string& account_name )
   {
      auto name_index_itr = name_to_account.find( account_name );
      if( name_index_itr != name_to_account.end() )
      {
         auto account_itr = accounts.find( name_index_itr->second );
         FC_ASSERT( account_itr != accounts.end(),
                    "wallet database in inconsistant state" ); 
         return account_itr->second;
      }
      return owallet_account_record();
   }
   void wallet_db::cache_balance( const bts::blockchain::balance_record& balance_to_cache )
   {
      auto balance_id = balance_to_cache.id();
      owallet_balance_record current_bal = lookup_balance(balance_id);
      if( !current_bal.valid() )
      {
         wallet_balance_record new_rec( balance_to_cache, new_index() );
         store_record( new_rec );
         balances[balance_id] = new_rec;
      }
      else
      {
         *current_bal = balance_to_cache;
         store_record( *current_bal );
         balances[balance_id] = *current_bal;
      }
   }

   void wallet_db::rename_account( const string& old_account_name, 
                                   const string& new_account_name )
   {
       FC_ASSERT( !"Not Implemented" );
   }

   void wallet_db::store_transaction( const signed_transaction& trx_to_store )
   { try {
      auto trx_id = trx_to_store.id();
      auto itr = transactions.find( trx_id );
      if( itr != transactions.end() ) return;

      wallet_transaction_record data;
      data.index = new_index();
      data.trx = trx_to_store;
      store_record( data );
      transactions[trx_id] = data;

   } FC_RETHROW_EXCEPTIONS( warn, "", ("trx_to_store",trx_to_store) ) }
   void wallet_db::cache_transaction( const signed_transaction& trx,
                                      const string& memo_message,
                                      const public_key_type& to )
   { try {
      auto trx_id = trx.id();
      auto itr = transactions.find( trx_id );
      if( itr != transactions.end() ) return;

      wallet_transaction_record data;
      data.index = new_index();
      data.trx = trx;
      data.to_account = to;
      data.memo_message = memo_message;
      store_record( data );
      transactions[trx_id] = data;
       
       //transaction_data data
   } FC_RETHROW_EXCEPTIONS( warn, "", 
            ("trx",trx)
            ("memo_message",memo_message)
            ("to",to) ) }

   void wallet_db::cache_account( const wallet_account_record& war )
   {
      accounts[war.index] = war;
      if( war.registered_account_id != 0 )
      {
         account_id_to_account[war.registered_account_id] = war.index;
      }
      store_record( war );
   }

} } // bts::wallet
