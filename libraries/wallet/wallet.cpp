#include <bts/wallet/wallet.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/db/level_map.hpp>
#include <fc/crypto/aes.hpp>
#include <fc/crypto/base58.hpp>
#include <bts/import_bitcoin_wallet.hpp>

#include <iostream>

#define EXTRA_PRIVATE_KEY_BASE (100*1000*1000ll)
//#define ACCOUNT_INDEX_BASE     (200*1000*1000ll)


namespace bts { namespace wallet {

   namespace detail 
   {
      class wallet_impl : public bts::blockchain::chain_observer
      {
         public:
            wallet_impl()
            {
               // this effectively sets the maximum transaction size to 10KB without
               // custom code.  We will set this initial fee high while the network
               // is of lower value to prevent initial spam attacks.   We also
               // want to subsidize the delegates earlly on.
               _current_fee = asset( 1000*100, 0 );
               _data_dir = ".";
            }

            virtual void state_changed( const pending_chain_state_ptr& applied_changes ) override
            {
               for( auto account : applied_changes->accounts )
               {
                  scan_account( account.second );         
               }
               for( auto current_asset : applied_changes->assets )
               {
                  scan_asset( current_asset.second );
               }
               for( auto name : applied_changes->names )
               {
                  scan_name( name.second );
               }
            }
            /**
             *  This method is called anytime a block is applied to the chain.
             */
            virtual void block_applied( const block_summary& summary ) override
            {
               state_changed( summary.applied_changes );
               for( auto trx : summary.block_data.user_transactions )
               {
                  scan_transaction( trx );
               }
            }

            wallet* self;

            asset _current_fee;

            bool           _is_open;
            fc::time_point _relock_time;
            fc::path       _data_dir;
            std::string    _wallet_name;

            /** meta_record_property_enum is the key */
            std::unordered_map<int,wallet_meta_record>                          _meta;

            chain_database_ptr                                                  _blockchain;

            /** map record id to encrypted record data , this db should only be written to via
             * my->store_record()  
             **/
            bts::db::level_map< uint32_t, wallet_record >                       _wallet_db;
            template<typename T>
            void store_record( const T& record )
            {
               _wallet_db.store( record.index, wallet_record(record) );
            }
                                                                                
            /** the password required to decrypt the wallet records */          
            fc::sha512                                                          _wallet_password;
                                                                                
            fc::optional<master_key_record>                                     _master_key;
                                                                                
            /** lookup contact state */                                         
            std::unordered_map<uint32_t,wallet_contact_record>                  _contacts;
                                                                                
            /** registered accounts */                                             
            std::unordered_map<account_id_type,wallet_account_record>           _accounts;
                                                                                
            /** registered names */                                             
            std::unordered_map<name_id_type,wallet_name_record>                 _names;
                                                                                
            /** registered assets */                                            
            std::unordered_map<asset_id_type,wallet_asset_record>               _assets;

            /** all transactions to or from this wallet */
            std::unordered_map<transaction_id_type,wallet_transaction_record>   _transactions;

            /** caches all addresses and where in the hierarchial tree they can be found */
            std::unordered_map<address, hkey_index>                             _receive_keys;

            std::string get_address_label( const hkey_index& idx )
            {
               auto contact_itr = _contacts.find( idx.contact_num );
               if( contact_itr != _contacts.end() )
                  return contact_itr->second.name;
               return std::string();
            }

            /** used when hkey_index == (X,-1,N) to lookup foreign private keys, where
             * N is the key into _extra_receive_keys
             **/
            std::unordered_map<int32_t, private_key_record >                   _extra_receive_keys;

            std::unordered_map<std::string, uint32_t>                          _contact_name_index;


            int32_t get_new_index()
            {
               auto meta_itr = _meta.find( next_record_number );
               if( meta_itr == _meta.end() )
               {
                  _meta[next_record_number] = wallet_meta_record( 0, next_record_number, 1 );
                  store_record( _meta[next_record_number] );
                  return 1;
               }
               auto next_index = meta_itr->second.value.as_int64() + 1;
               _meta[next_record_number].value = next_index;
               FC_ASSERT( _meta[next_record_number].index == 0 );
               store_record( _meta[next_record_number] );
               return next_index;
            }

            uint32_t get_last_scanned_block_number()
            {
               auto meta_itr = _meta.find( last_scanned_block_number );
               if( meta_itr == _meta.end() )
               {
                  wallet_meta_record rec( get_new_index(), last_scanned_block_number, -1 ); 
                  _meta[last_scanned_block_number] = rec;
                  store_record( rec );
                  return -1;
               }
               return meta_itr->second.value.as_int64();
            }
            void set_last_scanned_block_number( uint32_t num )
            {
               auto meta_itr = _meta.find( last_scanned_block_number );
               if( meta_itr == _meta.end() )
               {
                  wallet_meta_record rec( get_new_index(), last_scanned_block_number, int64_t(num) ); 
                  _meta[last_scanned_block_number] = rec;
                  store_record( rec );
               }
               else
               {
                  meta_itr->second.value = num;
                  store_record( meta_itr->second );
               }
            }

            /** contact indexes are tracked independently from record indexes because
             * the goal is to focus them early in the hierarchial wallet number 
             * sequence to make recovery more feasible.
             */
            int32_t get_next_contact_index()
            {
                auto meta_itr = _meta.find( last_contact_index );
                if( meta_itr == _meta.end() )
                {
                   auto new_index = get_new_index();
                   _meta[last_contact_index] = wallet_meta_record( new_index, last_contact_index, 1 );
                   store_record( _meta[last_contact_index] );
                   return 1;
                }
                auto next_index = meta_itr->second.value.as_int64() + 1;
                meta_itr->second.value = next_index;
                store_record(meta_itr->second );
                return next_index;
            }

            asset  get_default_fee()
            {
                auto meta_itr = _meta.find( default_transaction_fee );
                if( meta_itr == _meta.end() )
                {
                   auto new_index = get_new_index();
                   _meta[default_transaction_fee] = wallet_meta_record( new_index, 
                                                                        default_transaction_fee, 
                                                                        fc::variant(asset( 1000*100, 0)) );
                   store_record( _meta[default_transaction_fee] );
                   return _meta[default_transaction_fee].value.as<asset>();
                }
                return meta_itr->second.value.as<asset>();
            }

            int32_t get_last_contact_index()
            {
                auto meta_itr = _meta.find( last_contact_index );
                if( meta_itr == _meta.end() )
                {
                   auto new_index = get_new_index();
                   _meta[last_contact_index] = wallet_meta_record( new_index, last_contact_index, 0 );
                   store_record( _meta[last_contact_index] );
                   return 0;
                }
                return meta_itr->second.value.as_int64();
            }

            void initialize_wallet( const std::string& password )
            {
               _wallet_password = fc::sha512::hash( password.c_str(), password.size() );
               FC_ASSERT( password.size() > 0 , "No wallet password specified" );

               auto key = fc::ecc::private_key::generate();
               auto chain_code = fc::ecc::private_key::generate();

               extended_private_key exp( key.get_secret(), chain_code.get_secret() );
              
               _master_key = master_key_record();
               _master_key->index = get_new_index();
               _master_key->encrypted_key = fc::aes_encrypt( _wallet_password, fc::raw::pack(exp) );
               _master_key->checksum = fc::sha512::hash( _wallet_password );

               store_record( *_master_key  );
            }

            /** the key index that the account belongs to */
            void index_account( const hkey_index& idx, const account_record& account )
            {
               ilog( "index account ${a} ${idx}", ("a",account)("idx",idx) );
               auto id = account.id();
               auto itr = _accounts.find( id );
               if( itr == _accounts.end() )
               {
                  _accounts[id] = wallet_account_record( get_new_index(), account );
                  itr = _accounts.find( id );
               }
               else
               {
                  itr->second = wallet_account_record( itr->second.index, account );
               }
               store_record( itr->second );
            }

            void index_name( const hkey_index& idx, const name_record& name )
            {
                auto itr = _names.find( name.id );
                if( itr != _names.end() )
                {
                   itr->second = wallet_name_record( itr->second.index, name );
                }
                else
                {
                   _names[name.id] = wallet_name_record( get_new_index(), name );
                   itr = _names.find( name.id );
                }
                store_record( itr->second );
            }

            void index_asset( const asset_record& a )
            {
                auto itr = _assets.find( a.id );
                if( itr != _assets.end() )
                {
                   itr->second = wallet_asset_record( itr->second.index, a );
                }
                else
                {
                   _assets[a.id] = wallet_asset_record( get_new_index(), a );
                   itr = _assets.find( a.id );
                }
                store_record( itr->second );
            }


            void scan_account( const account_record& account )
            {
                switch( (withdraw_condition_types)account.condition.condition )
                {
                   case withdraw_signature_type:
                   {
                      auto owner = account.condition.as<withdraw_with_signature>().owner;
                      if( is_my_address( owner ) )
                      {
                          index_account( _receive_keys.find( owner )->second, account );
                      }
                      break;
                   }
                   case withdraw_multi_sig_type:
                   {
                      auto cond = account.condition.as<withdraw_with_multi_sig>();
                      for( auto owner : cond.owners )
                      {
                         if( is_my_address( owner ) )
                         {
                            index_account( _receive_keys.find( owner )->second, account );
                            break;
                         }
                      }
                   }
                   case withdraw_password_type:
                   {
                      auto cond = account.condition.as<withdraw_with_password>();
                      auto itr = _receive_keys.find( cond.payee );
                      if( itr != _receive_keys.end() )
                      {
                         index_account( itr->second, account );
                         break;
                      }
                      itr = _receive_keys.find( cond.payor );
                      if( itr != _receive_keys.end() )
                      {
                         index_account( itr->second, account );
                         break;
                      }
                   }
                   case withdraw_option_type:
                   {
                      auto cond = account.condition.as<withdraw_option>();
                      auto itr = _receive_keys.find( cond.optionor );
                      if( itr != _receive_keys.end() )
                      {
                         index_account( itr->second, account );
                         break;
                      }
                      itr = _receive_keys.find( cond.optionee );
                      if( itr != _receive_keys.end() )
                      {
                         index_account( itr->second, account );
                         break;
                      }
                      break;
                   }
                   case withdraw_null_type:
                      break;
                }
            }

            void scan_asset( const asset_record& current_asset )
            {
                if( _names.find( current_asset.issuer_name_id ) != _names.end() )
                {

                }
            }

            void scan_name( const name_record& name )
            {
               auto current_itr = _names.find( name.id );
               if( current_itr != _names.end() )
               {
                  // do we still own it or has it been transferred to someone else.
               }
               else
               {
                  if( self->is_my_address( address( name.owner_key ) ) || self->is_my_address( address( name.active_key) ) )
                  {
                     _names[name.id] = wallet_name_record( get_new_index(), name );
                     store_record( _names[name.id] );
                  }
                  else
                  {
                    wlog( "not our name ${r}", ("r",name) );
                  }
               }
            }

            bool scan_withdraw( const withdraw_operation& op )
            {
               auto account_itr = _accounts.find( op.account_id );
               if( account_itr != _accounts.end() )
               {
                  auto account_rec = _blockchain->get_account_record(op.account_id);
                  if( !!account_rec ) scan_account( *account_rec );
                  return true;
               }
               return false;
            }
            bool is_my_address( const address& a ) { return self->is_my_address( a ); }
            bool scan_deposit( const deposit_operation& op )
            {
               switch( (withdraw_condition_types) op.condition.condition )
               {
                  case withdraw_signature_type:
                     return self->is_my_address( op.condition.as<withdraw_with_signature>().owner );
                  case withdraw_multi_sig_type:
                  {
                     for( auto owner : op.condition.as<withdraw_with_multi_sig>().owners )
                        if( self->is_my_address( owner ) ) return true;
                     break;
                  }
                  case withdraw_password_type:
                  {
                     auto cond = op.condition.as<withdraw_with_password>();
                     if( is_my_address( cond.payee ) ) return true;
                     if( is_my_address( cond.payor ) ) return true;
                     break;
                  }
                  case withdraw_option_type:
                  {
                     auto cond = op.condition.as<withdraw_option>();
                     if( is_my_address( cond.optionor ) ) return true;
                     if( is_my_address( cond.optionee ) ) return true;
                     break;
                  }
                  case withdraw_null_type:
                     break;
               }
               return false;
            }


            bool scan_reserve_name( const reserve_name_operation& op )
            {
               if( is_my_address( op.owner_key ) ) return true;
               if( is_my_address( op.active_key ) ) return true;
               return false;
            }

            bool scan_update_name( const update_name_operation& op )
            {
               return _names.find( op.name_id ) != _names.end();
            }

            bool scan_create_asset( const create_asset_operation& op )
            {
               return _names.find( op.issuer_name_id ) != _names.end();
            }
            bool scan_update_asset( const update_asset_operation& op )
            {
               return _assets.find( op.asset_id ) != _assets.end();
            }
            bool scan_issue_asset( const issue_asset_operation& op )
            {
               return _assets.find( op.asset_id ) != _assets.end();
            }

            void scan_transaction( const signed_transaction& trx )
            {
                bool mine = false;
                for( auto op : trx.operations )
                {
                   switch( (operation_type_enum)op.type  )
                   {
                     case withdraw_op_type:
                        mine |= scan_withdraw( op.as<withdraw_operation>() );
                        break;
                     case deposit_op_type:
                     {
                        auto dop = op.as<deposit_operation>();
                        if( scan_deposit( dop ) )
                        {
                           auto account_rec = _blockchain->get_account_record(dop.account_id());
                           if( !!account_rec ) scan_account( *account_rec );
                           mine = true;
                        }
                        break;
                     }
                     case reserve_name_op_type:
                        mine |= scan_reserve_name( op.as<reserve_name_operation>() );
                        break;
                     case update_name_op_type:
                        mine |= scan_update_name( op.as<update_name_operation>() );
                        break;
                     case create_asset_op_type:
                        mine |= scan_create_asset( op.as<create_asset_operation>() );
                        break;
                     case update_asset_op_type:
                        mine |= scan_update_asset( op.as<update_asset_operation>() );
                        break;
                     case issue_asset_op_type:
                        mine |= scan_issue_asset( op.as<issue_asset_operation>() );
                        break;
                     case null_op_type:
                        break;
                   }
                }
                if( mine )
                {
                   store_transaction( trx );
                }
            }

            void store_transaction( const signed_transaction& trx )
            {
               auto trx_id = trx.id();
               auto trx_rec_itr = _transactions.find( trx_id );
               if( trx_rec_itr != _transactions.end() )
               {
                  trx_rec_itr->second.trx = trx;
               }
               else
               {
                  _transactions[trx_id] = wallet_transaction_record( get_new_index(), trx );
                  trx_rec_itr = _transactions.find( trx_id );
               }   
               store_record( trx_rec_itr->second );
            }

            void withdraw_to_transaction( signed_transaction& trx, 
                                          const asset& amount, std::unordered_set<address>& required_sigs )
            { try {
                asset total_in( 0, amount.asset_id );
                asset total_left = amount;
                for( auto record : _accounts )
                {
                   if( record.second.condition.asset_id == amount.asset_id )
                   {
                      if( record.second.balance > 0 )
                      {
                         required_sigs.insert( record.second.condition.as<withdraw_with_signature>().owner );

                         auto withdraw_amount = std::min( record.second.balance, total_left.amount );
                         record.second.balance -= withdraw_amount;

                         ilog( "withdraw amount ${a} ${total_left}", 
                               ("a",withdraw_amount)("total_left",total_left) );
                         trx.withdraw( record.first, withdraw_amount );
                         total_left.amount -= withdraw_amount;

                         if( total_left.amount == 0 ) return;
                      }
                   }
                }
                if( total_left.amount > 0 ) FC_ASSERT( !"Insufficient Funds" );
            } FC_RETHROW_EXCEPTIONS( warn, "", ("amount", amount) ) }

            fc::ecc::private_key get_private_key( const hkey_index& index )
            { try {
                //if( index.address_num < 0 )
                elog( "${index}", ("index",index) );
                if( index.trx_num == -1 )
                {
                    auto priv_key_rec_itr = _extra_receive_keys.find( index.address_num );
                    if( priv_key_rec_itr == _extra_receive_keys.end() )
                    {
                        FC_ASSERT( !"Unable to find private key for address" );
                    }
                    return priv_key_rec_itr->second.get_private_key( _wallet_password );
                }
                else
                {
                    auto priv_key = _master_key->get_extended_private_key( _wallet_password );
                    return priv_key.child( index.contact_num, extended_private_key::private_derivation )
                                   .child( index.trx_num, extended_private_key::public_derivation )
                                   .child( index.address_num, extended_private_key::public_derivation );

                }
            } FC_RETHROW_EXCEPTIONS( warn, "", ("index",index) ) }

            bool check_address( const fc::ecc::public_key& k, const address& addr )
            { 
                if( address(k) == addr ) return true;
                if( address(pts_address(k,false,0) )  == addr ) return true;  
                if( address(pts_address(k,false,56) ) == addr ) return true;  
                if( address(pts_address(k,true,0) )   == addr ) return true;  
                if( address(pts_address(k,true,56) )  == addr ) return true;  
                return false;
            }

            fc::ecc::private_key get_private_key( const address& pub_key_addr )
            { try {
                auto key_index_itr = _receive_keys.find( pub_key_addr );
                FC_ASSERT( key_index_itr != _receive_keys.end() );
                auto priv_key = get_private_key( key_index_itr->second );
                FC_ASSERT( check_address( priv_key.get_public_key(), pub_key_addr), "",
                           ("pub_key",priv_key.get_public_key())("addr",pub_key_addr));
                return priv_key;

            } FC_RETHROW_EXCEPTIONS( warn, "", ("address",pub_key_addr) ) }


            void sign_transaction( signed_transaction& trx, const std::unordered_set<address>& required_sigs )
            { try {
                for( auto item : required_sigs )
                {
                   auto priv_key = get_private_key( item );
                   trx.sign( priv_key );
                }
            } FC_RETHROW_EXCEPTIONS( warn, "", ("trx",trx)("required",required_sigs) ) }

      }; // wallet_impl

   } // namespace detail

   wallet::wallet( const chain_database_ptr& chain_db )
   :my( new detail::wallet_impl() )
   {
      my->self = this;
      my->_blockchain = chain_db;
      chain_db->set_observer( my.get() );
   }

   wallet::~wallet(){}


   void wallet::open( const std::string& wallet_name, const std::string& password )
   { try {
      try {
         close();
         open_file( my->_data_dir / wallet_name, password );
         my->_wallet_name = wallet_name;
      } catch ( ... ) { my->_wallet_name = ""; throw; }
   } FC_RETHROW_EXCEPTIONS( warn, "", ("wallet_name",wallet_name) )
   }

   void wallet::create( const std::string& wallet_name, const std::string& password )
   { try {
         auto filename = my->_data_dir / wallet_name;
         FC_ASSERT( !fc::exists( filename ), "Wallet ${wallet_dir} already exists.", ("wallet_dir",filename) )
         my->_wallet_db.open( filename, true );
         if( !my->_master_key )
         {
            my->initialize_wallet( password );
         }
         close();
         open( wallet_name, password );
   } FC_RETHROW_EXCEPTIONS( warn, "unable to create wallet with name ${name}", ("name",wallet_name) ) }

   void wallet::open_file( const fc::path& wallet_dir, const std::string& password )
   { try {
      FC_ASSERT( fc::exists( wallet_dir ), "Unable to open ${wallet_dir}  ${password}", ("wallet_dir",wallet_dir)("password",password) )

      std::cout << "Opening wallet " << wallet_dir.generic_string() << "\n";

      my->_wallet_password = fc::sha512::hash( password.c_str(), password.size() );
      my->_wallet_db.open( wallet_dir, true );

      auto record_itr = my->_wallet_db.begin();
      while( record_itr.valid() )
      {
         auto record = record_itr.value();
         wlog( "${k}] wallet record: ${r}:", ("r",record)("k",record_itr.key()) );
         try {
            switch( (wallet_record_type)record.type )
            {
               case master_key_record_type:
               {
                  my->_master_key = record.as<master_key_record>();
                  unlock( password );
                  break;
               }
               case contact_record_type:
               {
                  auto cr = record.as<wallet_contact_record>();
                  my->_contacts[cr.contact_num] = cr;
                  my->_contact_name_index[cr.name] = cr.index;

                //  if( my->get_last_contact_index() < cr.index )
                //     my->_last_contact_index = cr.index;
                  break;
               }
               case transaction_record_type: 
               {
                  auto wtr    = record.as<wallet_transaction_record>();
                  auto trx_id = wtr.trx.id();
                  my->_transactions[trx_id] = wtr;
                  break;
               }
               case name_record_type:
               {
                  auto wnr = record.as<wallet_name_record>();
                  my->_names[wnr.id] = wnr;
                  break;
               }
               case asset_record_type:
               {
                  auto wnr = record.as<wallet_asset_record>();
                  my->_assets[wnr.id] = wnr;
                  break;
               }
               case account_record_type:
               {
                  auto war = record.as<wallet_account_record>();
                  my->_accounts[war.id()] = war;
                  break;
               }
               case private_key_record_type:
               {
                  auto pkr = record.as<private_key_record>();
                  my->_extra_receive_keys[pkr.extra_key_index] = pkr;
                  auto pubkey = pkr.get_private_key(my->_wallet_password).get_public_key();
                  elog( "public key: ${key}", ("key",pubkey) );
                  my->_receive_keys[ address( pubkey ) ] = 
                     hkey_index( pkr.contact_index, -1, pkr.extra_key_index );
                  my->_receive_keys[ address(pts_address(pubkey,false,56) )] = 
                     hkey_index( pkr.contact_index, -1, pkr.extra_key_index );
                  my->_receive_keys[ address(pts_address(pubkey,true,56) ) ] = 
                     hkey_index( pkr.contact_index, -1, pkr.extra_key_index );
                  my->_receive_keys[ address(pts_address(pubkey,false,0) ) ] = 
                     hkey_index( pkr.contact_index, -1, pkr.extra_key_index );
                  my->_receive_keys[ address(pts_address(pubkey,true,0) )  ] = 
                     hkey_index( pkr.contact_index, -1, pkr.extra_key_index );
                  break;
               }
               case meta_record_type:
               {
                  auto metar = record.as<wallet_meta_record>();
                  my->_meta[metar.key] = metar;
                  break;
               }
            }
         } 
         catch ( const fc::exception& e )
         {
            elog( "error loading wallet record: ${e}", ("e",e.to_detail_string() ) );
            // TODO: log errors and report them to user...
         }
         ++record_itr;
      }
      FC_ASSERT( !!my->_master_key, "No master key found in wallet" )
      my->_current_fee = my->get_default_fee();
      scan_chain( my->get_last_scanned_block_number() );

      my->_is_open = true;
      lock();

   } FC_RETHROW_EXCEPTIONS( warn, "unable to open wallet '${file}'", ("file",wallet_dir) ) }

   bool wallet::is_open()const { return my->_is_open; }

   void wallet::lock()
   {
      my->_wallet_password = fc::sha512();
   }
   std::string wallet::get_name()const
   {
      return my->_wallet_name;
   }
   bool wallet::is_locked()const { return !is_unlocked(); }
   bool wallet::is_unlocked()const
   {
      return my->_wallet_password != fc::sha512();
   }

   bool wallet::close()
   { try {
      my->_wallet_db.close();
      my->_wallet_password = fc::sha512();
      my->_master_key.reset();
      my->_contacts.clear();
      my->_accounts.clear();
      my->_names.clear();
      my->_assets.clear();
      my->_transactions.clear();
      my->_receive_keys.clear();
      my->_extra_receive_keys.clear();
      my->_contact_name_index.clear();
      my->_meta.clear();
      my->_wallet_name = "";
      my->_relock_time = fc::time_point();
      my->_is_open = false;
      return true;
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   bool wallet::unlock( const std::string& password, const fc::microseconds& timeout )
   { try {
      FC_ASSERT( password.size() > 0 );
      FC_ASSERT( !!my->_master_key );
      my->_wallet_password = fc::sha512::hash( password.c_str(), password.size() );

      if( my->_master_key->checksum != fc::sha512::hash( my->_wallet_password ) ) 
      {
         my->_wallet_password = fc::sha512();
         return false;
      }
      my->_relock_time = fc::time_point::now() + timeout;
      return true;
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }


   wallet_contact_record wallet::create_contact( const std::string& name, 
                                                 const extended_public_key& contact_pub_key )
   { try {
        auto current_itr = my->_contact_name_index.find( name );
        FC_ASSERT( current_itr == my->_contact_name_index.end() );
        FC_ASSERT( is_unlocked() );

        wallet_contact_record wcr;
        wcr.index             = my->get_new_index(); 
        wcr.contact_num       = my->get_next_contact_index(); //++my->_last_contact_index;
        wcr.name              = name;
        wcr.extended_send_key = contact_pub_key;
        wlog( "creating contact '${name}'", ("name",wcr) );


        auto master_key = my->_master_key->get_extended_private_key(my->_wallet_password);
        wcr.extended_recv_key = master_key.child( wcr.contact_num );

        my->_contact_name_index[name] = wcr.contact_num;
        my->_contacts[wcr.contact_num] = wcr;
        my->store_record( wcr );
        return wcr;
   } FC_RETHROW_EXCEPTIONS( warn, "unable to create contact", ("name",name)("ext_pub_key", contact_pub_key) ) }

   void wallet::set_contact_extended_send_key( const std::string& name, 
                                               const extended_public_key& contact_pub_key )
   { try {
        auto current_itr = my->_contact_name_index.find(name);
        FC_ASSERT( current_itr != my->_contact_name_index.end() );
        auto current_wcr = my->_contacts.find( current_itr->second );
        FC_ASSERT( current_wcr != my->_contacts.end() );
        current_wcr->second.extended_send_key = contact_pub_key;

        my->store_record( current_wcr->second );
   } FC_RETHROW_EXCEPTIONS( warn, "unable to create contact", ("name",name)("ext_pub_key", contact_pub_key) ) }

   std::vector<std::string> wallet::get_contacts()const
   {
      std::vector<std::string> cons;
      cons.reserve( my->_contact_name_index.size() );
      for( auto item : my->_contact_name_index )
         cons.push_back( item.first );
      return cons;
   }

   void wallet::import_private_key( const fc::ecc::private_key& priv_key, const std::string& contact_name )
   {
       auto contact_itr = my->_contact_name_index.find( contact_name );
       if( contact_itr != my->_contact_name_index.end() )
       {
          auto contact_record_itr = my->_contacts.find( contact_itr->second );
          FC_ASSERT( contact_record_itr != my->_contacts.end() );
          import_private_key( priv_key, contact_record_itr->second.contact_num );
       }
       else
       {
          create_contact( contact_name );
          import_private_key( priv_key, contact_name );
       }
   }
   void wallet::import_private_key( const fc::ecc::private_key& priv_key, int32_t contact_index )
   {
      FC_ASSERT( is_unlocked() );
      auto key = priv_key.get_public_key();
      address pub_address( key );
      if( my->_receive_keys.find( pub_address ) != my->_receive_keys.end() )
      {
         wlog( "duplicate import of key ${a}", ("a",pub_address) );
         return;
      }
      int32_t key_num = my->_extra_receive_keys.size();
      auto record_id = my->get_new_index();
      auto pkr = private_key_record( record_id, contact_index, key_num, priv_key, my->_wallet_password );
      elog( "contact_index: ${i}", ("i",contact_index) );
      my->_extra_receive_keys[key_num] = pkr;
      my->_receive_keys[pub_address] = hkey_index( contact_index, -1, key_num );
      my->_receive_keys[ address(pts_address(key,false,56) )] = hkey_index( contact_index, -1, key_num );
      my->_receive_keys[ address(pts_address(key,true,56) ) ] = hkey_index( contact_index, -1, key_num );
      my->_receive_keys[ address(pts_address(key,false,0) ) ] = hkey_index( contact_index, -1, key_num );
      my->_receive_keys[ address(pts_address(key,true,0) )  ] = hkey_index( contact_index, -1, key_num );

      my->store_record( pkr ); 
   }

   void wallet::scan_state()
   {
      my->_blockchain->scan_accounts( [=]( const account_record& rec )
      {
          std::cout << std::string(rec.id()) << "  " << rec.balance << "\n";
          my->scan_account( rec );
      });
      my->_blockchain->scan_names( [=]( const name_record& rec )
      {
          my->scan_name( rec );
      });
   }

   void wallet::scan( const block_summary& summary )
   {
      for( auto trx : summary.block_data.user_transactions )
      {
     //    my->scan_transaction_state( trx_state );
      }
   }
   asset wallet::get_balance( asset_id_type asset_id  )
   {
      asset balance(0,asset_id);
      for( auto item : my->_accounts )
         balance += item.second.get_balance( asset_id );
      return balance;
   }

   wallet_contact_record wallet::get_contact( uint32_t contact_num )
   { try {
      auto itr = my->_contacts.find( contact_num );
      FC_ASSERT( itr != my->_contacts.end() );
      return itr->second;
   } FC_RETHROW_EXCEPTIONS( warn, "unable to find contact", ("contact_num",contact_num) ) }

   address  wallet::get_new_address( const std::string& name )
   { try {
      return address( get_new_public_key( name ) );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("name",name) ) }

   public_key_type  wallet::get_new_public_key( const std::string& name )
   { try {
      FC_ASSERT( is_unlocked() );
      auto contact_name_itr = my->_contact_name_index.find( name );
      if( contact_name_itr != my->_contact_name_index.end() )
      {
         auto contact = get_contact( contact_name_itr->second );
         auto hindex  = contact.get_next_receive_key_index( 0 );
         wlog( "hindex: ${h}", ("h",hindex) );
         my->_contacts[ contact_name_itr->second ] = contact;
         my->store_record( contact ); 

         auto priv_key = my->get_private_key( hindex );
         auto pub_key  = priv_key.get_public_key();
         address pub_addr(pub_key);
         my->_receive_keys[ pub_addr ] = hindex;

         return pub_key;
      }
      else
      {
         wlog( "create contact for '${name}'", ("name",name) );
         create_contact( name );
         return get_new_public_key( name );
      }
   } FC_RETHROW_EXCEPTIONS( warn, "", ("name",name) ) }

   signed_transaction wallet::send_to_address( const asset& amount, const address& owner, const std::string& memo )
   { try {
     FC_ASSERT( is_unlocked() );

     std::unordered_set<address> required_sigs;

     signed_transaction trx;
     if( amount.asset_id == my->_current_fee.asset_id )
     {
        my->withdraw_to_transaction( trx, amount + my->_current_fee, required_sigs );
     }
     else
     {
        my->withdraw_to_transaction( trx, amount, required_sigs );
        my->withdraw_to_transaction( trx, my->_current_fee, required_sigs );
     }

     name_id_type delegate_id = rand()%BTS_BLOCKCHAIN_DELEGATES + 1;
     trx.deposit( owner, amount, delegate_id );
     my->sign_transaction( trx, required_sigs );

     return trx;
   } FC_RETHROW_EXCEPTIONS( warn, "", ("amount",amount)("owner",owner) ) }


   signed_transaction wallet::update_name( const std::string& name, 
                                           fc::optional<fc::variant> json_data, 
                                           fc::optional<public_key_type> active, 
                                           bool as_delegate )
   { try {
      auto name_rec = my->_blockchain->get_name_record( name );
      FC_ASSERT( !!name_rec );

      std::unordered_set<address> required_sigs;
      signed_transaction trx;

      asset total_fees = my->_current_fee;

      if( !!active && *active != name_rec->active_key ) 
         required_sigs.insert( name_rec->owner_key );
      else
         required_sigs.insert( name_rec->active_key );

      auto current_fee_rate = my->_blockchain->get_fee_rate();

      fc::optional<std::string> ojson_str;
      if( !!json_data )
      {
         std::string json_str = fc::json::to_string( *json_data );
         FC_ASSERT( json_str.size() < BTS_BLOCKCHAIN_MAX_NAME_DATA_SIZE );
         FC_ASSERT( name_record::is_valid_json( json_str ) );
         total_fees += asset( (json_str.size() * current_fee_rate)/1000, 0 );
         ojson_str = json_str;
      }
      if( !as_delegate && name_rec->is_delegate )
         FC_ASSERT( !"You cannot unregister as a delegate" );

      if( !name_rec->is_delegate && as_delegate )
      {
        total_fees += asset( (BTS_BLOCKCHAIN_DELEGATE_REGISTRATION_FEE*current_fee_rate)/1000, 0 );
      }

      my->withdraw_to_transaction( trx, total_fees, required_sigs );
      trx.update_name( name_rec->id, ojson_str, active, as_delegate );
      my->sign_transaction( trx, required_sigs );

      return trx;
   } FC_RETHROW_EXCEPTIONS( warn, "", ("name",name)("json",json_data)("active",active)("as_delegate",as_delegate) ) }

   signed_transaction wallet::reserve_name( const std::string& name, const fc::variant& json_data, bool as_delegate )
   { try {
      FC_ASSERT( name_record::is_valid_name( name ), "", ("name",name) );
      auto name_rec = my->_blockchain->get_name_record( name );
      FC_ASSERT( !name_rec, "Name '${name}' has already been reserved", ("name",name)  );

      std::unordered_set<address> required_sigs;
      signed_transaction trx;

      asset total_fees = my->_current_fee;

      auto current_fee_rate = my->_blockchain->get_fee_rate();
      if( as_delegate )
      {
        total_fees += asset( (BTS_BLOCKCHAIN_DELEGATE_REGISTRATION_FEE*current_fee_rate)/1000, 0 );
      }
      auto json_str = fc::json::to_string(json_data);
      total_fees += asset( (json_str.size() * current_fee_rate)/1000, 0 );

      my->withdraw_to_transaction( trx, total_fees, required_sigs );

      auto owner_key  = get_new_public_key();
      auto active_key = get_new_public_key();
      trx.reserve_name( name, json_str, owner_key, active_key );

      my->sign_transaction( trx, required_sigs );

      return trx;
   } FC_RETHROW_EXCEPTIONS( warn, "", ("name",name)("data",json_data)("delegate",as_delegate) ) }

   bool wallet::is_my_address( const address& addr )const
   {
      auto itr = my->_receive_keys.find( addr );
      return itr != my->_receive_keys.end();
   }

   void wallet::sign_block( signed_block_header& header )const
   { try {
      FC_ASSERT( is_unlocked() );
      auto delegate_pub_key = my->_blockchain->get_signing_delegate_key( header.timestamp );
      auto delegate_key = my->get_private_key( delegate_pub_key );
      FC_ASSERT( delegate_pub_key == delegate_key.get_public_key() );

      ilog( "delegate_pub_key: ${key}", ("key",delegate_pub_key) );

      header.sign(delegate_key);
      FC_ASSERT( header.validate_signee( delegate_pub_key ) );

   } FC_RETHROW_EXCEPTIONS( warn, "", ("header",header) ) }

   /**
    *  If this wallet is a delegate, calculate the time that it should produce the next 
    *  block.
    *
    *  @see @ref dpos_block_producer
    */
   fc::time_point_sec wallet::next_block_production_time()const
   {
      fc::time_point_sec now = fc::time_point::now();
      int64_t interval_number = now.sec_since_epoch() / BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
      int64_t round_start = (interval_number / BTS_BLOCKCHAIN_DELEGATES) * BTS_BLOCKCHAIN_DELEGATES;

      fc::time_point_sec next_time;

      auto sorted_delegates = my->_blockchain->get_delegates_by_vote();
      for( uint32_t i = 0; i < sorted_delegates.size(); ++i )
      {
         auto name_itr = my->_names.find( sorted_delegates[i] );
         if( name_itr != my->_names.end() )
         {
             if( (round_start + i) * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC < now.sec_since_epoch() )
             {
                fc::time_point_sec tmp((round_start + i + BTS_BLOCKCHAIN_DELEGATES) * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC);
                if( tmp < next_time || next_time == fc::time_point_sec() )
                   next_time = tmp;
             }
             else
             {
                fc::time_point_sec tmp((round_start + i) * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC);
                if( tmp < next_time || next_time == fc::time_point_sec() )
                   next_time = tmp;
             }
         }
      }
      return next_time;
   }

   std::unordered_map<address,std::string> wallet::get_send_addresses()const
   {
      std::unordered_map<address,std::string> result;
      return result;
   }

   std::unordered_map<address,std::string> wallet::get_receive_addresses()const
   {
      std::unordered_map<address,std::string> result;
      for( auto item : my->_receive_keys )
      {
          result[item.first] = my->get_address_label( item.second );
      }
      return result;
   }
   void wallet::import_bitcoin_wallet( const fc::path& wallet_dat, const std::string& passphrase )
   { try {
      auto priv_keys = bts::import_bitcoin_wallet(  wallet_dat, passphrase );
      for( auto key : priv_keys )
      {
         import_private_key( key, wallet_dat.filename().generic_string() );
      }
   } FC_RETHROW_EXCEPTIONS( warn, "Unable to import bitcoin wallet ${wallet_dat}", ("wallet_dat",wallet_dat) ) }

   void wallet::import_wif_key( const std::string& wif, const std::string& contact_name )
   { try {
      auto wif_bytes = fc::from_base58(wif);
      auto key = fc::variant(std::vector<char>(wif_bytes.begin() + 1, wif_bytes.end() - 4)).as<fc::ecc::private_key>();
      import_private_key(key, contact_name);
   } FC_RETHROW_EXCEPTIONS( warn, "unable to import wif private key" ) }


   void wallet::scan_chain( uint32_t block_num, scan_progress_callback cb  )
   { try {
       uint32_t head_block_num = my->_blockchain->get_head_block_num();
       if( head_block_num != uint32_t(-1) )
       {
          for( uint32_t i = block_num; i <= head_block_num; ++i )
          {
             auto blk = my->_blockchain->get_block( i );
             scan_block( blk );
             cb( i, head_block_num, 0, 0 );
             my->set_last_scanned_block_number( i );
          }
       }
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   void wallet::scan_block( const full_block& blk )
   {
      for( auto trx : blk.user_transactions )
         my->scan_transaction( trx );
   }

   /**
    *  Sets the directory where wallets will be created when using username / password
    */
   void wallet::set_data_directory( const fc::path& data_dir )
   {
      my->_data_dir = data_dir;
   }
   void  wallet::add_sending_address( const address&, const std::string& label )
   {
      FC_ASSERT( !"add_sending_address is not implemented yet" );
   }
   void  wallet::add_sending_address( const address&, int32_t contact_index  )
   {
      FC_ASSERT( !"add_sending_address is not implemented yet" );
   }
   std::unordered_map<transaction_id_type,wallet_transaction_record>  wallet::transactions()const
   {
      return my->_transactions;
   }

} } // bts::wallet
