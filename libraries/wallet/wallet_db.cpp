#include <bts/blockchain/time.hpp>
#include <bts/db/level_map.hpp>
#include <bts/wallet/wallet_db.hpp>

#include <fc/io/json.hpp>
#include <fstream>

namespace bts { namespace wallet {

   using namespace bts::blockchain;

   namespace detail {
     class wallet_db_impl
     {
        public:
           wallet_db*                                        self;
           bts::db::level_map<int32_t,generic_wallet_record> _records;

           void store_and_reload_generic_record( const generic_wallet_record& record )
           { try {
               auto index = record.get_wallet_record_index();
               FC_ASSERT( index != 0 );
               FC_ASSERT( _records.is_open() );
               _records.store( index, record );
               load_generic_record( record );
           } FC_CAPTURE_AND_RETHROW( (record) ) }

           void load_generic_record( const generic_wallet_record& record )
           { try {
               switch( wallet_record_type_enum( record.type ) )
               {
                   case master_key_record_type:
                       load_master_key_record( record.as<wallet_master_key_record>() );
                       break;
                   case account_record_type:
                       load_account_record( record.as<wallet_account_record>() );
                       break;
                   case key_record_type:
                       load_key_record( record.as<wallet_key_record>() );
                       break;
                   case transaction_record_type:
                       load_transaction_record( record.as<wallet_transaction_record>() );
                       break;
                   case balance_record_type:
                       load_balance_record( record.as<wallet_balance_record>() );
                       break;
                   case property_record_type:
                       load_property_record( record.as<wallet_property_record>() );
                       break;
                   case setting_record_type:
                       load_setting_record( record.as<wallet_setting_record>() );
                       break;
                   default:
                       elog( "Unknown wallet record type: ${type}", ("type",record.type) );
                       break;
                }
           } FC_CAPTURE_AND_RETHROW( (record) ) }

           void load_master_key_record( const wallet_master_key_record& key )
           { try {
              self->wallet_master_key = key;
           } FC_CAPTURE_AND_RETHROW() }

           void load_account_record( const wallet_account_record& account_record )
           { try {
               self->accounts[ account_record.wallet_record_index ] = account_record;

               // Cache address map
               self->address_to_account_wallet_record_index[ account_record.account_address ] = account_record.wallet_record_index;
               for( const auto& item : account_record.active_key_history )
               {
                   const public_key_type& active_key = item.second;
                   self->address_to_account_wallet_record_index[ address( active_key ) ] = account_record.wallet_record_index;
               }

               // Cache name map
               self->name_to_account_wallet_record_index[ account_record.name ] = account_record.wallet_record_index;

               // Cache id map
               if( account_record.id != 0 )
                   self->account_id_to_wallet_record_index[ account_record.id ] = account_record.wallet_record_index;
           } FC_CAPTURE_AND_RETHROW( (account_record) ) }

           void load_key_record( const wallet_key_record& key_record )
           { try {
               const address key_address = key_record.get_address();

               self->keys[ key_address ] = key_record;

               // Cache address map
               self->btc_to_bts_address[ key_address ] = key_address;
               self->btc_to_bts_address[ address( pts_address( key_record.public_key, false, 0  ) ) ] = key_address; // Uncompressed BTC
               self->btc_to_bts_address[ address( pts_address( key_record.public_key, true,  0  ) ) ] = key_address; // Compressed BTC
               self->btc_to_bts_address[ address( pts_address( key_record.public_key, false, 56 ) ) ] = key_address; // Uncompressed PTS
               self->btc_to_bts_address[ address( pts_address( key_record.public_key, true,  56 ) ) ] = key_address; // Compressed PTS
           } FC_CAPTURE_AND_RETHROW( (key_record) ) }

           void load_transaction_record( const wallet_transaction_record& transaction_record )
           { try {
               self->transactions[ transaction_record.record_id ] = transaction_record;
           } FC_CAPTURE_AND_RETHROW( (transaction_record) ) }

           void load_balance_record( const wallet_balance_record& rec )
           { try {
              self->balances[ rec.id() ] = rec;
           } FC_CAPTURE_AND_RETHROW( (rec) ) }

           void load_property_record( const wallet_property_record& property_rec )
           { try {
              self->properties[property_rec.key] = property_rec;
           } FC_CAPTURE_AND_RETHROW( (property_rec) ) }

           void load_setting_record( const wallet_setting_record& rec )
           { try {
              self->settings[rec.name] = rec;
           } FC_CAPTURE_AND_RETHROW( (rec) ) }
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
      try
      {
          my->_records.open( wallet_file, true );
          for( auto itr = my->_records.begin(); itr.valid(); ++itr )
          {
             auto record = itr.value();
             try
             {
                my->load_generic_record( record );
                // prevent hanging on large wallets
                fc::usleep( fc::microseconds(1000) );
             }
             catch (const fc::canceled_exception&)
             {
                throw;
             }
             catch ( const fc::exception& e )
             {
                wlog( "Error loading wallet record:\n${r}\nreason: ${e}", ("e",e.to_detail_string())("r",record) );
             }
          }
      }
      catch( ... )
      {
          close();
          throw;
      }
   } FC_RETHROW_EXCEPTIONS( warn, "Error opening wallet file ${file}", ("file",wallet_file) ) }

   void wallet_db::close()
   {
      my->_records.close();

      wallet_master_key.reset();

      accounts.clear();
      keys.clear();
      transactions.clear();
      balances.clear();
      properties.clear();
      settings.clear();

      address_to_account_wallet_record_index.clear();
      name_to_account_wallet_record_index.clear();
      account_id_to_wallet_record_index.clear();

      btc_to_bts_address.clear();
   }

   bool wallet_db::is_open()const
   {
       return my->_records.is_open() && wallet_master_key.valid();
   }

   void wallet_db::store_and_reload_generic_record( const generic_wallet_record& record )
   {
       FC_ASSERT( my->_records.is_open() );
       my->store_and_reload_generic_record( record );
   }

   int32_t wallet_db::new_wallet_record_index()
   {
      auto next_rec_num = get_property( next_record_number );
      int32_t next_rec_number = 2;
      if( next_rec_num.is_null() )
      {
         next_rec_number = 2;
      }
      else
      {
         next_rec_number = next_rec_num.as<int32_t>();
      }
      set_property( property_enum::next_record_number, next_rec_number + 1 );
      return next_rec_number;
   }

   uint32_t wallet_db::get_last_wallet_child_key_index()const
   { try {
       FC_ASSERT( is_open() );
       try
       {
           return get_property( next_child_key_index ).as<uint32_t>();
       }
       catch( ... )
       {
       }
       return 0;
   } FC_CAPTURE_AND_RETHROW() }

   void wallet_db::set_last_wallet_child_key_index( uint32_t key_index )
   { try {
       FC_ASSERT( is_open() );
       set_property( next_child_key_index, key_index );
   } FC_CAPTURE_AND_RETHROW( (key_index) ) }

   private_key_type wallet_db::get_wallet_child_key( const fc::sha512& password, uint32_t key_index )const
   { try {
       FC_ASSERT( is_open() );
       const extended_private_key master_private_key = wallet_master_key->decrypt_key( password );
       return master_private_key.child( key_index );
   } FC_CAPTURE_AND_RETHROW( (key_index) ) }

   public_key_type wallet_db::generate_new_account( const fc::sha512& password, const string& account_name, const variant& private_data )
   { try {
       FC_ASSERT( is_open() );

       owallet_account_record account_record = lookup_account( account_name );
       FC_ASSERT( !account_record.valid(), "Wallet already contains an account with that name!" );

       uint32_t key_index = get_last_wallet_child_key_index();
       private_key_type account_private_key;
       public_key_type account_public_key;
       address account_address;
       while( true )
       {
           ++key_index;
           FC_ASSERT( key_index != 0, "Overflow!" );

           account_private_key = get_wallet_child_key( password, key_index );
           account_public_key = account_private_key.get_public_key();
           account_address = address( account_public_key );

           account_record = lookup_account( account_address );
           if( account_record.valid() ) continue;

           owallet_key_record key_record = lookup_key( account_address );
           if( key_record.valid() && key_record->has_private_key() ) continue;

           break;
       }

       key_data key;
       key.account_address = account_address;
       key.public_key = account_public_key;
       key.encrypt_private_key( password, account_private_key );
       key.gen_seq_number = key_index;

       account_data account;
       account.name = account_name;
       account.owner_key = account_public_key;
       account.set_active_key( blockchain::now(), account_public_key );
       account.account_address = account_address;
       account.private_data = private_data;
       account.is_my_account = true;

       set_last_wallet_child_key_index( key_index );
       store_key( key );
       store_account( account );

       return account_public_key;
   } FC_CAPTURE_AND_RETHROW( (account_name) ) }

   private_key_type wallet_db::get_account_child_key( const private_key_type& active_private_key, uint32_t seq_num )const
   { try {
       FC_ASSERT( is_open() );
       const extended_private_key extended_active_private_key = extended_private_key( active_private_key );
       fc::sha256::encoder enc;
       fc::raw::pack( enc, seq_num );
       return extended_active_private_key.child( enc.result() );
   } FC_CAPTURE_AND_RETHROW( (seq_num) ) }

   // Deprecated but kept for key regeneration
   private_key_type wallet_db::get_account_child_key_v1( const fc::sha512& password, const address& account_address, uint32_t seq_num )const
   { try {
       FC_ASSERT( is_open() );
       const extended_private_key master_private_key = wallet_master_key->decrypt_key( password );
       fc::sha256::encoder enc;
       fc::raw::pack( enc, account_address );
       fc::raw::pack( enc, seq_num );
       return master_private_key.child( enc.result() );
   } FC_CAPTURE_AND_RETHROW( (account_address)(seq_num) ) }

   private_key_type wallet_db::generate_new_account_child_key( const fc::sha512& password, const string& account_name )
   { try {
       FC_ASSERT( is_open() );

       owallet_account_record account_record = lookup_account( account_name );
       FC_ASSERT( account_record.valid(), "Account not found!" );
       FC_ASSERT( account_record->is_my_account, "Not my account!" );

       const owallet_key_record key_record = lookup_key( address( account_record->active_key() ) );
       FC_ASSERT( key_record.valid(), "Active key not found!" );
       FC_ASSERT( key_record->has_private_key(), "Active private key not found!" );

       const private_key_type active_private_key = key_record->decrypt_private_key( password );
       uint32_t seq_num = account_record->last_used_gen_sequence;
       private_key_type account_child_private_key;
       public_key_type account_child_public_key;
       address account_child_address;
       while( true )
       {
           ++seq_num;
           FC_ASSERT( seq_num != 0, "Overflow!" );

           account_child_private_key = get_account_child_key( active_private_key, seq_num );
           account_child_public_key = account_child_private_key.get_public_key();
           account_child_address = address( account_child_public_key );

           owallet_key_record key_record = lookup_key( account_child_address );
           if( key_record.valid() && key_record->has_private_key() ) continue;

           break;
       }

       account_record->last_used_gen_sequence = seq_num;

       key_data key;
       key.account_address = account_record->account_address;
       key.public_key = account_child_public_key;
       key.encrypt_private_key( password, account_child_private_key );
       key.gen_seq_number = seq_num;

       store_account( *account_record );
       store_key( key );

       return account_child_private_key;
   } FC_CAPTURE_AND_RETHROW( (account_name) ) }

   void wallet_db::add_contact_account( const account_record& blockchain_account_record, const variant& private_data )
   { try {
       FC_ASSERT( is_open() );

       owallet_account_record record = lookup_account( blockchain_account_record.name );
       FC_ASSERT( !record.valid(), "Wallet already contains an account with that name!" );

       const public_key_type account_address = blockchain_account_record.owner_key;
       owallet_key_record key_record = lookup_key( account_address );
       FC_ASSERT( !key_record.valid(), "Wallet already contains that key" );

       record = wallet_account_record();
       account_record& temp_record = *record;
       temp_record = blockchain_account_record;
       record->account_address = account_address;
       record->private_data = private_data;

       store_account( *record );
   } FC_CAPTURE_AND_RETHROW( (blockchain_account_record) ) }

   owallet_account_record wallet_db::lookup_account( const address& account_address )const
   { try {
       FC_ASSERT( is_open() );
       const auto address_map_iter = address_to_account_wallet_record_index.find( account_address );
       if( address_map_iter != address_to_account_wallet_record_index.end() )
       {
           const int32_t& record_index = address_map_iter->second;
           const auto record_iter = accounts.find( record_index );
           if( record_iter != accounts.end() )
           {
               const wallet_account_record& account_record = record_iter->second;
               return account_record;
           }
       }
       return owallet_account_record();
   } FC_CAPTURE_AND_RETHROW( (account_address) ) }

   owallet_account_record wallet_db::lookup_account( const string& account_name )const
   { try {
       FC_ASSERT( is_open() );
       const auto name_map_iter = name_to_account_wallet_record_index.find( account_name );
       if( name_map_iter != name_to_account_wallet_record_index.end() )
       {
           const int32_t& record_index = name_map_iter->second;
           const auto record_iter = accounts.find( record_index );
           if( record_iter != accounts.end() )
           {
               const wallet_account_record& account_record = record_iter->second;
               return account_record;
           }
       }
       return owallet_account_record();
   } FC_CAPTURE_AND_RETHROW( (account_name) ) }

   owallet_account_record wallet_db::lookup_account( const account_id_type& account_id )const
   { try {
       FC_ASSERT( is_open() );
       const auto id_map_iter = account_id_to_wallet_record_index.find( account_id );
       if( id_map_iter != account_id_to_wallet_record_index.end() )
       {
           const int32_t& record_index = id_map_iter->second;
           const auto record_iter = accounts.find( record_index );
           if( record_iter != accounts.end() )
           {
               const wallet_account_record& account_record = record_iter->second;
               return account_record;
           }
       }
       return owallet_account_record();
   } FC_CAPTURE_AND_RETHROW( (account_id) ) }

   void wallet_db::store_account( const account_data& account )
   { try {
       FC_ASSERT( is_open() );
       FC_ASSERT( account.name != string() );
       FC_ASSERT( account.owner_key != public_key_type() );
       FC_ASSERT( account.active_key() != public_key_type() );
       FC_ASSERT( account.account_address == address( account.owner_key ) );

       owallet_account_record account_record = lookup_account( account.account_address );
       if( !account_record.valid() )
           account_record = wallet_account_record();

       account_data& temp = *account_record;
       temp = account;

       store_and_reload_record( *account_record );

       set<public_key_type> account_public_keys;
       account_public_keys.insert( account_record->owner_key );
       for( const auto& active_key_item : account_record->active_key_history )
       {
           const public_key_type& active_key = active_key_item.second;
           account_public_keys.insert( active_key );
       }

       for( const public_key_type& account_public_key : account_public_keys )
       {
           const address account_address = address( account_public_key );
           const owallet_key_record key_record = lookup_key( account_address );
           if( !key_record.valid() )
           {
               key_data key;
               key.account_address = account_address;
               key.public_key = account_public_key;
               store_key( key );
           }
       }
   } FC_CAPTURE_AND_RETHROW( (account) ) }

   void wallet_db::store_account( const blockchain::account_record& blockchain_account_record )
   { try {
       FC_ASSERT( is_open() );

       const address account_address = address( blockchain_account_record.owner_key );
       owallet_account_record account_record = lookup_account( account_address );
       if( !account_record.valid() )
           account_record = wallet_account_record();

       blockchain::account_record& temp = *account_record;
       temp = blockchain_account_record;

       account_record->account_address = account_address;

       store_account( *account_record );
   } FC_CAPTURE_AND_RETHROW( (blockchain_account_record) ) }

   owallet_key_record wallet_db::lookup_key( const address& derived_address )const
   { try {
       FC_ASSERT( is_open() );
       const auto address_map_iter = btc_to_bts_address.find( derived_address );
       if( address_map_iter != btc_to_bts_address.end() )
       {
           const address& key_address = address_map_iter->second;
           const auto record_iter = keys.find( key_address );
           if( record_iter != keys.end() )
           {
               const wallet_key_record& key_record = record_iter->second;
               return key_record;
           }
       }
       return owallet_key_record();
   } FC_CAPTURE_AND_RETHROW( (derived_address) ) }

   void wallet_db::store_key( const key_data& key )
   { try {
       FC_ASSERT( is_open() );
       FC_ASSERT( key.public_key != public_key_type() );

       owallet_key_record key_record = lookup_key( key.get_address() );
       if( !key_record.valid() )
           key_record = wallet_key_record();

       key_data& temp = *key_record;
       temp = key;

       store_and_reload_record( *key_record );

       if( key_record->has_private_key() )
       {
           owallet_account_record account_record = lookup_account( key.account_address );
           if( account_record.valid() && !account_record->is_my_account )
           {
               account_record->is_my_account = true;
               store_account( *account_record );
           }
       }
   } FC_CAPTURE_AND_RETHROW( (key) ) }

   void wallet_db::import_key( const fc::sha512& password, const string& account_name, const private_key_type& private_key )
   { try {
       FC_ASSERT( is_open() );

       owallet_account_record account_record = lookup_account( account_name );
       FC_ASSERT( account_record.valid(), "Account name not found!" );

       const public_key_type public_key = private_key.get_public_key();
       const address key_address = address( public_key );

       owallet_key_record key_record = lookup_key( key_address );
       if( key_record.valid() )
           FC_ASSERT( key_record->account_address == account_record->account_address );
       else
           key_record = wallet_key_record();

       key_record->account_address = account_record->account_address;
       key_record->public_key = public_key;
       key_record->encrypt_private_key( password, private_key );

       store_key( *key_record );
   } FC_CAPTURE_AND_RETHROW( (account_name) ) }

   owallet_transaction_record wallet_db::lookup_transaction( const transaction_id_type& record_id )const
   { try {
       FC_ASSERT( is_open() );
       const auto id_map_iter = transactions.find( record_id );
       if( id_map_iter != transactions.end() )
       {
           const wallet_transaction_record& transaction_record = id_map_iter->second;
           return transaction_record;
       }
       return owallet_transaction_record();
   } FC_CAPTURE_AND_RETHROW( (record_id) ) }

   void wallet_db::store_transaction( const transaction_data& transaction )
   { try {
       FC_ASSERT( is_open() );
       FC_ASSERT( transaction.record_id != transaction_id_type() );
       FC_ASSERT( transaction.is_virtual || transaction.trx.id() != signed_transaction().id() );

       owallet_transaction_record transaction_record = lookup_transaction( transaction.record_id );
       if( !transaction_record.valid() )
           transaction_record = wallet_transaction_record();

       transaction_data& temp = *transaction_record;
       temp = transaction;

       store_and_reload_record( *transaction_record );
   } FC_CAPTURE_AND_RETHROW( (transaction) ) }

   private_key_type wallet_db::generate_new_one_time_key( const fc::sha512& password )
   { try {
       FC_ASSERT( is_open() );
       const private_key_type one_time_private_key = private_key_type::generate();

       const address one_time_address = address( one_time_private_key.get_public_key() );
       const owallet_key_record key_record = lookup_key( one_time_address );
       FC_ASSERT( !key_record.valid() );

       key_data key;
       key.public_key = one_time_private_key.get_public_key();
       key.encrypt_private_key( password, one_time_private_key );
       store_key( key );

       return one_time_private_key;
   } FC_CAPTURE_AND_RETHROW() }

   map<private_key_type, string> wallet_db::get_account_private_keys( const fc::sha512& password )const
   { try {
       map<public_key_type, string> public_keys;
       for( const auto& account_item : accounts )
       {
           const auto& account = account_item.second;
           public_keys[ account.owner_key ] = account.name;
           for( const auto& active_key_item : account.active_key_history )
           {
               const auto& active_key = active_key_item.second;
               public_keys[ active_key ] = account.name;
           }
       }

       map<private_key_type, string> private_keys;
       for( const auto& public_key_item : public_keys )
       {
           const auto& public_key = public_key_item.first;
           const auto& account_name = public_key_item.second;

           const auto key_record = lookup_key( public_key );
           if( !key_record.valid() || !key_record->has_private_key() )
               continue;

           try
           {
               private_keys[ key_record->decrypt_private_key( password ) ] = account_name;
           }
           catch( const fc::exception& e )
           {
               elog( "Error decrypting private key: ${e}", ("e",e.to_detail_string()) );
           }
       }
       return private_keys;
   } FC_CAPTURE_AND_RETHROW() }

   void wallet_db::repair_records( const fc::sha512& password )
   { try {
       FC_ASSERT( is_open() );

       vector<generic_wallet_record> records;
       for( auto iter = my->_records.begin(); iter.valid(); ++iter )
           records.push_back( iter.value() );

       // Repair account_data.account_address
       for( generic_wallet_record& record : records )
       {
           try
           {
               if( wallet_record_type_enum( record.type ) == account_record_type )
               {
                   wallet_account_record account_record = record.as<wallet_account_record>();
                   account_record.account_address = address( account_record.owner_key );
                   store_account( account_record );
               }
           }
           catch( ... )
           {
           }
       }

       // Repair key_data.public_key when I have the private key
       for( generic_wallet_record& record : records )
       {
           try
           {
               if( wallet_record_type_enum( record.type ) == key_record_type )
               {
                   wallet_key_record key_record = record.as<wallet_key_record>();
                   if( key_record.has_private_key() )
                   {
                       const private_key_type private_key = key_record.decrypt_private_key( password );
                       key_record.public_key = private_key.get_public_key();
                       store_key( key_record );
                   }
               }
           }
           catch( ... )
           {
           }
       }

       // Repair transaction_data.record_id
       for( generic_wallet_record& record : records )
       {
           try
           {
               if( wallet_record_type_enum( record.type ) == transaction_record_type )
               {
                   wallet_transaction_record transaction_record = record.as<wallet_transaction_record>();
                   if( transaction_record.trx.id() != signed_transaction().id()  )
                   {
                       remove_item( transaction_record.wallet_record_index );
                       transactions.erase( transaction_record.record_id );
                       transaction_record.record_id = transaction_record.trx.id();
                       store_transaction( transaction_record );
                   }
               }
           }
           catch( ... )
           {
           }
       }
   } FC_CAPTURE_AND_RETHROW() }

   void wallet_db::set_property( property_enum property_id, const variant& v )
   {
      wallet_property_record property_record;
      auto property_itr = properties.find( property_id );
      if( property_itr != properties.end() )
      {
          property_record = property_itr->second;
          property_record.value = v;
      }
      else
      {
          if( property_id == property_enum::next_record_number )
              property_record = wallet_property_record( wallet_property(property_id, v), 1 );
          else
              property_record = wallet_property_record( wallet_property(property_id, v), new_wallet_record_index() );
      }
      store_and_reload_record( property_record );
   }

   variant wallet_db::get_property( property_enum property_id )const
   {
      auto property_itr = properties.find( property_id );
      if( property_itr != properties.end() ) return property_itr->second.value;
      return variant();
   }

   string wallet_db::get_account_name( const address& account_address )const
   {
      auto opt = lookup_account( account_address );
      if( opt ) return opt->name;
      return "?";
   }

   void wallet_db::store_setting(const string& name, const variant& value)
   {
       auto orec = lookup_setting(name);
       if (orec.valid())
       {
           orec->value = value;
           settings[name] = *orec;
           store_and_reload_record( *orec );
       }
       else
       {
           auto rec = wallet_setting_record( setting(name, value), new_wallet_record_index() );
           settings[name] = rec;
           store_and_reload_record( rec );
       }
   }

   vector<wallet_transaction_record> wallet_db::get_pending_transactions()const
   {
       vector<wallet_transaction_record> transaction_records;
       for( const auto& item : transactions )
       {
           const auto& transaction_record = item.second;
           if( !transaction_record.is_virtual && !transaction_record.is_confirmed )
               transaction_records.push_back( transaction_record );
       }
       return transaction_records;
   }

   void wallet_db::export_to_json( const path& filename )const
   { try {
      FC_ASSERT( is_open() );
      FC_ASSERT( !fc::exists( filename ) );

      const auto dir = fc::absolute( filename ).parent_path();
      if( !fc::exists( dir ) )
          fc::create_directories( dir );

      std::ofstream fs( filename.string() );
      fs.write( "[\n", 2 );

      auto itr = my->_records.begin();
      while( itr.valid() )
      {
          auto str = fc::json::to_pretty_string( itr.value() );
          if( (++itr).valid() ) str += ",";
          str += "\n";
          fs.write( str.c_str(), str.size() );
      }

      fs.write( "]", 1 );
   } FC_CAPTURE_AND_RETHROW( (filename) ) }

   void wallet_db::import_from_json( const path& filename )
   { try {
      FC_ASSERT( fc::exists( filename ) );
      FC_ASSERT( is_open() );

      auto records = fc::json::from_file<std::vector<generic_wallet_record>>( filename );
      for( const auto& record : records )
         store_and_reload_generic_record( record );
   } FC_CAPTURE_AND_RETHROW( (filename) ) }

   bool wallet_db::has_private_key( const address& addr )const
   { try {
      auto itr = keys.find( addr );
      if( itr != keys.end() )
      {
         return itr->second.has_private_key();
      }
      return false;
   } FC_CAPTURE_AND_RETHROW( (addr) ) }

   void wallet_db::cache_memo( const memo_status& memo,
                               const private_key_type& account_key,
                               const fc::sha512& password )
   {
      key_data data;
      data.account_address = address( account_key.get_public_key() );
      data.public_key = memo.owner_private_key.get_public_key();
      data.encrypt_private_key( password, memo.owner_private_key );
      data.valid_from_signature = memo.has_valid_signature;
      //data.memo = memo_data( memo );
      store_key( data );
   }

   owallet_balance_record wallet_db::lookup_balance( const balance_id_type& balance_id )const
   {
      auto itr = balances.find( balance_id );
      if( itr == balances.end() ) return fc::optional<wallet_balance_record>();
      return itr->second;
   }

   vector<wallet_balance_record> wallet_db::get_all_balances( const string& account_name, uint32_t limit )
   {
       auto ret = vector<wallet_balance_record>();
       auto count = 0;
       for( auto item : balances )
       {
           if (count == limit && limit != -1)
               break;
           auto okey = lookup_key(item.second.owner());
           FC_ASSERT(okey.valid(), "expect a key record to exist at this point");
           auto oacct = lookup_account( okey->account_address );
           if ( oacct.valid() && oacct->name == account_name )
           {
               ret.push_back(item.second);
               count++;
           }
       }
       return ret;
   }

   owallet_setting_record wallet_db::lookup_setting( const string& name)const
   {
      auto itr = settings.find(name);
      if (itr != settings.end() )
      {
          return itr->second;
      }
      return owallet_setting_record();
   }

   void wallet_db::cache_balance( const bts::blockchain::balance_record& balance_to_cache )
   {
      auto balance_id = balance_to_cache.id();
      owallet_balance_record current_bal = lookup_balance(balance_id);
      wallet_balance_record balance_record;
      if( !current_bal.valid() )
      {
         balance_record = wallet_balance_record( balance_to_cache, new_wallet_record_index() );
      }
      else
      {
         blockchain::balance_record& chain_rec = *current_bal;
         chain_rec = balance_to_cache;
         balance_record = *current_bal;
      }

      store_and_reload_record( balance_record );
   }

   void wallet_db::remove_contact_account( const string& account_name )
   {
      const auto opt_account = lookup_account( account_name );
      FC_ASSERT( opt_account.valid() );
      const auto acct = *opt_account;
      FC_ASSERT( ! has_private_key(address(acct.owner_key)), "you can only remove contact accounts");

      for( const auto& time_key_pair : acct.active_key_history )
      {
          keys.erase( address(time_key_pair.second) );
          address_to_account_wallet_record_index.erase( address(time_key_pair.second) );
      }
      remove_item( acct.wallet_record_index );

      keys.erase( address(acct.owner_key) );
      address_to_account_wallet_record_index.erase( address(acct.owner_key) );
      account_id_to_wallet_record_index.erase( acct.id );
      name_to_account_wallet_record_index.erase( account_name );
      accounts.erase( acct.wallet_record_index );
   }

   void wallet_db::rename_account(const public_key_type &old_account_key,
                                   const string& new_account_name )
   {
      /* Precondition: check that new_account doesn't exist in wallet and that old_account does
       */
      FC_ASSERT( is_open() );

      auto opt_old_acct = lookup_account( old_account_key );
      FC_ASSERT( opt_old_acct.valid() );
      auto acct = *opt_old_acct;
      auto old_name = acct.name;
      acct.name = new_account_name;
      name_to_account_wallet_record_index[acct.name] = acct.wallet_record_index;
      if( name_to_account_wallet_record_index[old_name] == acct.wallet_record_index )
        //Only remove the old name from the map if it pointed to the record we've just renamed
        name_to_account_wallet_record_index.erase( old_name );
      accounts[acct.wallet_record_index] = acct;
      address_to_account_wallet_record_index[address(acct.owner_key)] = acct.wallet_record_index;
      for( const auto& time_key_pair : acct.active_key_history )
          address_to_account_wallet_record_index[address(time_key_pair.second)] = acct.wallet_record_index;

      store_and_reload_record( acct );
   }

   void wallet_db::remove_item( int32_t index )
   { try {
       try
       {
           my->_records.remove( index );
       }
       catch( const fc::key_not_found_exception& )
       {
           wlog("wallet_db tried to remove nonexistent index: ${i}", ("i",index) );
       }
   } FC_CAPTURE_AND_RETHROW( (index) ) }

   bool wallet_db::validate_password( const fc::sha512& password )const
   { try {
      FC_ASSERT( wallet_master_key );
      return wallet_master_key->validate_password( password );
   } FC_CAPTURE_AND_RETHROW() }

   void wallet_db::set_master_key( const extended_private_key& extended_key,
                                   const fc::sha512& new_password )
   { try {
      master_key key;
      key.encrypt_key(new_password,extended_key);
      auto key_record = wallet_master_key_record( key, -1 );
      store_and_reload_record( key_record );
   } FC_CAPTURE_AND_RETHROW() }

   void wallet_db::change_password( const fc::sha512& old_password,
                                    const fc::sha512& new_password )
   { try {
      FC_ASSERT( is_open() );
      const extended_private_key master_private_key = wallet_master_key->decrypt_key( old_password );
      set_master_key( master_private_key, new_password );

      for( auto key : keys )
      {
         if( key.second.has_private_key() )
         {
            auto priv_key = key.second.decrypt_private_key( old_password );
            key.second.encrypt_private_key( new_password, priv_key );
            store_and_reload_record( key.second );
         }
      }
   } FC_CAPTURE_AND_RETHROW() }

   void wallet_db::remove_balance( const balance_id_type& balance_id )
   {
      remove_item( balances[balance_id].wallet_record_index );
      balances.erase(balance_id);
   }

   void wallet_db::remove_transaction( const transaction_id_type& record_id )
   {
      const auto rec = lookup_transaction( record_id );
      if( !rec.valid() ) return;
      remove_item( rec->wallet_record_index );
      transactions.erase( record_id );
   }

} } // bts::wallet
