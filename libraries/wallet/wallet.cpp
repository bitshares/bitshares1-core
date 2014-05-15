#include <bts/wallet/wallet.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/db/level_map.hpp>
#include <fc/crypto/aes.hpp>
#include <fc/crypto/base58.hpp>
#include <bts/import_bitcoin_wallet.hpp>

#include <fc/thread/future.hpp>
#include <fc/thread/thread.hpp>

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
               _priority_fee = asset( 1000*100, 0 );
               _data_dir = ".";
            }

            virtual ~wallet_impl() override {}

            void cache_deterministic_keys( const wallet_account_record& account, int32_t invoice_number, int32_t payment_number )
            {
               if( invoice_number < 0 )
                  return;
               if( account.account_number >= 0 )
               {
                     uint32_t last = payment_number + 2;
                     for( uint32_t i = 0; i <= last; ++i )
                     {
                        ilog( "receive caching: ${a}.${i}.${p} as ${k}", ("a",account.account_number)("i",invoice_number)("p",i)("k",  address(account.get_key( invoice_number, i ))));
                        _receive_keys[ account.get_key( invoice_number, i ) ] = address_index( account.account_number, invoice_number, i );
                     }
               }
               else
               {
                     uint32_t last = payment_number + 2;
                     for( uint32_t i = 0; i <= last; ++i )
                     {
                        ilog( "send caching: ${a}.${i}.${p} as ${k}", ("a",account.account_number)("i",invoice_number)("p",i)("k",  address(account.get_key( invoice_number, i ))));
                        _sending_keys[ account.get_key( invoice_number, i ) ] = address_index( account.account_number, invoice_number, i );
                     }
               }
            }
            void cache_deterministic_keys( const wallet_account_record& account )
            {
                for( auto item : account.last_payment_index )
                {
                   cache_deterministic_keys( account, item.first, item.second );
                }
                auto itr = account.last_payment_index.rbegin();
                if( itr != account.last_payment_index.rend() )
                {
                   for( uint32_t i = 1; i <= 3; ++i )
                    cache_deterministic_keys( account, itr->first + i, 0 );
                }
            }

            virtual void state_changed( const pending_chain_state_ptr& applied_changes ) override
            {
               for( auto balance : applied_changes->balances )
               {
                  scan_balance( balance.second );
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
            wallet_account_record get_account( const std::string& account_name )
            { try {
               auto account_name_itr = _account_name_index.find(account_name);
               if( account_name_itr != _account_name_index.end() )
                  return get_account( account_name_itr->second );
               FC_ASSERT( false, "Unable to find account \"${account_name}\"", ("account_name",account_name) );
            } FC_RETHROW_EXCEPTIONS( warn, "" ) }

            wallet_account_record get_account( int32_t account_number )
            { try {
               auto itr = _accounts.find( account_number );
               FC_ASSERT( itr != _accounts.end() );
               return itr->second;
            } FC_RETHROW_EXCEPTIONS( warn, "unable to find account", ("account_number",account_number) ) }

            void get_new_payment_address_from_account( const std::string& to_account_name,
                                                       int32_t& sending_invoice_index,
                                                       int32_t& last_sending_payment_index,
                                                       address& payment_address )
            {
               auto account_rec           = get_account( to_account_name );
               sending_invoice_index      = account_rec.get_next_invoice_index();
               last_sending_payment_index = account_rec.last_payment_index[sending_invoice_index];

               public_key_type pub_key = account_rec.get_key( sending_invoice_index, last_sending_payment_index );
               payment_address = pub_key;

               cache_deterministic_keys( account_rec, sending_invoice_index, last_sending_payment_index );
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
            void import_private_key( const private_key_type& priv_key,
                                     int32_t account_number,
                                     const std::string& invoice_memo );

            wallet* self;

            asset _priority_fee;

            bool           _is_open;
            fc::time_point _relock_time;
            fc::future<void> _wallet_relocker_done;
            fc::path       _data_dir;
            std::string    _wallet_name;
            fc::path       _wallet_filename;

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

            /** lookup account state */
            std::unordered_map<int32_t,wallet_account_record>                   _accounts;

            /** registered accounts */
            std::unordered_map<address,wallet_balance_record>                   _balances;

            /** registered names */
            std::unordered_map<name_id_type,wallet_name_record>                 _names;

            /** registered assets */
            std::unordered_map<asset_id_type,wallet_asset_record>               _assets;

            /** all transactions to or from this wallet */
            std::unordered_map<transaction_id_type,wallet_transaction_record>   _transactions;

            /** caches all addresses and where in the hierarchial tree they can be found */
            std::unordered_map<address, address_index>                             _receive_keys;
            std::unordered_map<address, address_index>                             _sending_keys;

            std::string get_address_label( const address_index& idx )
            {
               auto account_itr = _accounts.find( idx.account_number );
               if( account_itr != _accounts.end() )
                  return account_itr->second.name;
               return std::string();
            }

            /** used when address_index == (X,-1,N) to lookup foreign private keys, where
             * N is the key into _extra_receive_keys
             **/
            std::unordered_map<int32_t, private_key_record >                   _extra_receive_keys;

            std::unordered_map<std::string, uint32_t>                          _account_name_index;


            int32_t get_new_index()
            {
               auto meta_itr = _meta.find( next_record_number );
               if( meta_itr == _meta.end() )
               {
                  _meta[next_record_number] = wallet_meta_record( 0, next_record_number, 1 );
                  store_record( _meta[next_record_number] );
                  return 1;
               }
               int32_t next_index = meta_itr->second.value.as<int32_t>() + 1;
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
               return meta_itr->second.value.as<uint32_t>();
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

            /** account indexes are tracked independently from record indexes because
             * the goal is to focus them early in the hierarchial wallet number
             * sequence to make recovery more feasible.
             */
            int32_t get_next_account_number()
            {
                auto meta_itr = _meta.find( last_account_number );
                if( meta_itr == _meta.end() )
                {
                   auto new_index = get_new_index();
                   _meta[last_account_number] = wallet_meta_record( new_index, last_account_number, 1 );
                   store_record( _meta[last_account_number] );
                   return 1;
                }
                int32_t next_index = meta_itr->second.value.as<int32_t>() + 1;
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

            int32_t get_last_account_number()
            {
                auto meta_itr = _meta.find( last_account_number );
                if( meta_itr == _meta.end() )
                {
                   auto new_index = get_new_index();
                   _meta[last_account_number] = wallet_meta_record( new_index, last_account_number, 0 );
                   store_record( _meta[last_account_number] );
                   return 0;
                }
                return meta_itr->second.value.as<int32_t>();
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
            void index_account( const address_index& idx, const balance_record& balance )
            {
               auto id  = balance.id();
               auto itr = _balances.find( id );
               if( itr == _balances.end() )
               {
                  _balances[id] = wallet_balance_record( get_new_index(), balance );
                  itr = _balances.find( id );
               }
               else
               {
                  itr->second = wallet_balance_record( itr->second.index, balance );
               }
               ilog( "index account ${wallet_name}   ${a} ${idx}\n\n  ${record}", ("wallet_name",_wallet_name)("a",balance)("idx",idx)("record",itr->second) );
               store_record( itr->second );
            }

            void index_name( const address_index& idx, const name_record& name )
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


            void scan_balance( const balance_record& account )
            {
                switch( (withdraw_condition_types)account.condition.condition )
                {
                   case withdraw_signature_type:
                   {
                      auto owner = account.condition.as<withdraw_with_signature>().owner;
                      if( is_receive_address( owner ) )
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
                         if( is_receive_address( owner ) )
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
                  if( self->is_receive_address( address( name.owner_key ) ) || self->is_receive_address( address( name.active_key) ) )
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
               auto balance_itr = _balances.find( op.balance_id );
               if( balance_itr != _balances.end() )
               {
                  auto balance_rec = _blockchain->get_balance_record(op.balance_id);
                  if( !!balance_rec ) scan_balance( *balance_rec );
                  return true;
               }
               return false;
            }
            bool is_receive_address( const address& a ) { return self->is_receive_address( a ); }
            bool scan_deposit( const deposit_operation& op )
            {
               switch( (withdraw_condition_types) op.condition.condition )
               {
                  case withdraw_signature_type:
                     return self->is_receive_address( op.condition.as<withdraw_with_signature>().owner );
                  case withdraw_multi_sig_type:
                  {
                     for( auto owner : op.condition.as<withdraw_with_multi_sig>().owners )
                        if( self->is_receive_address( owner ) ) return true;
                     break;
                  }
                  case withdraw_password_type:
                  {
                     auto cond = op.condition.as<withdraw_with_password>();
                     if( is_receive_address( cond.payee ) ) return true;
                     if( is_receive_address( cond.payor ) ) return true;
                     break;
                  }
                  case withdraw_option_type:
                  {
                     auto cond = op.condition.as<withdraw_option>();
                     if( is_receive_address( cond.optionor ) ) return true;
                     if( is_receive_address( cond.optionee ) ) return true;
                     break;
                  }
                  case withdraw_null_type:
                     break;
               }
               return false;
            }


            bool scan_reserve_name( const reserve_name_operation& op )
            {
               if( is_receive_address( op.owner_key ) ) return true;
               if( is_receive_address( op.active_key ) ) return true;
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
               ilog( "scan transaction ${wallet}  - ${trx}", ("wallet",_wallet_name)("trx",trx) );
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
                           auto balance_rec = _blockchain->get_balance_record(dop.balance_id());
                           if( !!balance_rec ) scan_balance( *balance_rec );
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
                for( auto record : _balances )
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

            fc::ecc::private_key get_private_key( const address_index& index )
            { try {
                //if( index.address_num < 0 )
                elog( "${index}", ("index",index) );
                if( index.invoice_number < 0 )
                {
                    auto priv_key_rec_itr = _extra_receive_keys.find( index.payment_number );
                    if( priv_key_rec_itr == _extra_receive_keys.end() )
                    {
                        FC_ASSERT( !"Unable to find private key for address" );
                    }
                    return priv_key_rec_itr->second.get_private_key( _wallet_password );
                }
                else
                {
                    auto priv_key = _master_key->get_extended_private_key( _wallet_password );
                    return priv_key.child( index.account_number, extended_private_key::private_derivation )
                                   .child( index.invoice_number, extended_private_key::public_derivation )
                                   .child( index.payment_number, extended_private_key::public_derivation );

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
         close();
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
      my->_wallet_filename = wallet_dir;

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
               case account_record_type:
               {
                  auto cr = record.as<wallet_account_record>();
                  my->_accounts[cr.account_number] = cr;
                  my->_account_name_index[cr.name] = cr.account_number;

                  my->cache_deterministic_keys( cr );
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
               case balance_record_type:
               {
                  auto war = record.as<wallet_balance_record>();
                  my->_balances[war.id()] = war;
                  break;
               }
               case private_key_record_type:
               {
                  auto pkr = record.as<private_key_record>();
                  my->_extra_receive_keys[pkr.extra_key_index] = pkr;
                  auto pubkey = pkr.get_private_key(my->_wallet_password).get_public_key();
                  elog( "public key: ${key}", ("key",pubkey) );
                  my->_receive_keys[ address( pubkey ) ] =
                     address_index( pkr.account_number, -1, pkr.extra_key_index );
                  my->_receive_keys[ address(pts_address(pubkey,false,56) )] =
                     address_index( pkr.account_number, -1, pkr.extra_key_index );
                  my->_receive_keys[ address(pts_address(pubkey,true,56) ) ] =
                     address_index( pkr.account_number, -1, pkr.extra_key_index );
                  my->_receive_keys[ address(pts_address(pubkey,false,0) ) ] =
                     address_index( pkr.account_number, -1, pkr.extra_key_index );
                  my->_receive_keys[ address(pts_address(pubkey,true,0) )  ] =
                     address_index( pkr.account_number, -1, pkr.extra_key_index );
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
      unlock( password );
      my->_priority_fee = my->get_default_fee();
      scan_chain( my->get_last_scanned_block_number() );

      my->_is_open = true;
      lock();

   } FC_RETHROW_EXCEPTIONS( warn, "unable to open wallet '${file}'", ("file",wallet_dir) ) }

   bool wallet::is_open()const                   { return my->_is_open;                         }
   void wallet::lock()                           { my->_wallet_password = fc::sha512(); my->_relock_time = fc::time_point();   }
   std::string wallet::get_name()const           { return my->_wallet_name;                     }
   fc::path    wallet::get_filename()const       { return my->_wallet_filename;                 }
   bool wallet::is_locked()const                 { return !is_unlocked();                       }
   bool wallet::is_unlocked()const               { return my->_wallet_password != fc::sha512(); }
   fc::time_point wallet::unlocked_until() const { return my->_relock_time; }

   bool wallet::close()
   { try {
      my->_wallet_db.close();
      my->_wallet_password = fc::sha512();
      my->_master_key.reset();
      my->_accounts.clear();
      my->_balances.clear();
      my->_names.clear();
      my->_assets.clear();
      my->_transactions.clear();
      my->_receive_keys.clear();
      my->_extra_receive_keys.clear();
      my->_account_name_index.clear();
      my->_meta.clear();
      my->_wallet_name = "";
      my->_wallet_filename = fc::path();
      my->_relock_time = fc::time_point();
      my->_is_open = false;
      return true;
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   /**
    * TODO
    * @todo remove sleep/wait loop for duration and use scheduled notifications to relock
    */
   void wallet::unlock( const std::string& password, const fc::microseconds& timeout )
   { try {
      FC_ASSERT( password.size() > 0 );
      FC_ASSERT( !!my->_master_key );
      my->_wallet_password = fc::sha512::hash( password.c_str(), password.size() );

      /** TODO: salt the password checksum with something */
      if( my->_master_key->checksum != fc::sha512::hash( my->_wallet_password ) )
      {
         my->_wallet_password = fc::sha512();
         FC_THROW("Incorrect passphrase");
      }
      fc::time_point requested_relocking_time = fc::time_point::now() + timeout;
      my->_relock_time = std::max(my->_relock_time, requested_relocking_time);
      if (!my->_wallet_relocker_done.valid() || my->_wallet_relocker_done.ready())
      {
        my->_wallet_relocker_done = fc::async([this](){
          for (;;)
          {
            if (fc::time_point::now() > my->_relock_time)
            {
              lock();
              return;
            }
            fc::usleep(fc::seconds(1));
          }
        });
      }
#if 0
      // change the above code to this version once task cancellation is fixed in fc
      // if we're currently unlocked and have a timer counting down,
      // kill it and starrt a new one
      if (my->_wallet_relocker_done.valid() && !my->_wallet_relocker_done.ready())
      {
        my->_wallet_relocker_done.cancel();
        try
        {
          my->_wallet_relocker_done.wait();
        }
        catch (fc::canceled_exception&)
        {
        }
      }

      // now schedule a function call to relock the wallet when the specified interval
      // elapses (unless we were already unlocked for a longer interval)
      fc::time_point desired_relocking_time = fc::time_point::now() + duration;
      if (desired_relocking_time > my->_wallet_relock_time)
        my->_wallet_relock_time = desired_relocking_time;
      my->_wallet_relocker_done = fc::async([this](){
        fc::time_point sleep_start_time = fc::time_point::now();
        if (my->_wallet_relock_time > sleep_start_time)
          fc::usleep(my->_wallet_relock_time - sleep_start_time);
        lock_wallet();
      });
#endif
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   void wallet::change_password( const std::string& new_password )
   {
      FC_ASSERT( is_unlocked() );
      FC_ASSERT( !"Not Implemented Yet" );
      // TODO: implement change password
      // iterate over all private key records and re-encrypt them
      // re-encrypt master private
   }

   wallet_account_record wallet::create_receive_account( const std::string& account_name )
   { try {
        auto current_itr = my->_account_name_index.find( account_name );
        FC_ASSERT( current_itr == my->_account_name_index.end() );
        FC_ASSERT( is_unlocked() );

        wallet_account_record wcr;
        wcr.index             = my->get_new_index();
        wcr.account_number    = my->get_next_account_number();
        wcr.name              = account_name;


        auto master_key = my->_master_key->get_extended_private_key(my->_wallet_password);
        wcr.extended_key = master_key.child( wcr.account_number );
        wlog( "creating account '${account_name}'", ("account_name",wcr) );

        my->_account_name_index[account_name] = wcr.account_number;
        my->_accounts[wcr.account_number] = wcr;
        my->store_record( wcr );

        my->cache_deterministic_keys( wcr, 0, 0 );
        my->cache_deterministic_keys( wcr, 1, 0 );
        return wcr;
   } FC_RETHROW_EXCEPTIONS( warn, "unable to create account", ("account_name",account_name) ) }

   void wallet::create_sending_account( const std::string& account_name,
                                        const extended_public_key& account_pub_key )
   { try {
        auto current_itr = my->_account_name_index.find(account_name);
        FC_ASSERT( current_itr == my->_account_name_index.end() );

        wallet_account_record account;
        account.index             =  my->get_new_index();
        account.account_number    = -my->get_next_account_number();
        account.name              =  account_name;

        account.extended_key = account_pub_key;
        wlog( "creating account '${account_name}'", ("account_name",account) );

        my->_account_name_index[account_name] = account.account_number;
        my->_accounts[account.account_number] = account;

        my->store_record( account );
        my->cache_deterministic_keys( account, 0, 0 );
        my->cache_deterministic_keys( account, 1, 0 );
   } FC_RETHROW_EXCEPTIONS( warn, "unable to create account", ("name",account_name)("ext_pub_key", account_pub_key) ) }

   std::map<std::string,extended_address> wallet::list_receive_accounts( uint32_t start, uint32_t count )const
   {
      std::map<std::string,extended_address> cons;
      for( auto item : my->_account_name_index )
         cons[item.first] = extended_address(my->get_account( item.second ).extended_key);
      return cons;
   }
   wallet_account_record    wallet::get_account( const std::string& account_name )const
   { try {
      auto itr = my->_account_name_index.find( account_name );
      if( itr == my->_account_name_index.end() )
         FC_ASSERT( false, "invalid account name '${account_name}'", ("account_name",account_name) );
      return my->get_account( itr->second );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("account_name",account_name) ) }

   std::map<std::string,extended_address> wallet::list_sending_accounts( uint32_t start, uint32_t count )const
   {
      std::map<std::string,extended_address> cons;
      for( auto item : my->_account_name_index )
         cons[item.first] = extended_address(my->get_account( item.second ).extended_key);
      return cons;
   }

   void wallet::import_private_key( const fc::ecc::private_key& priv_key,
                                    const std::string& account_name,
                                    const std::string& invoice_memo )
   { try {
       auto account_itr = my->_account_name_index.find( account_name );
       if( account_itr != my->_account_name_index.end() )
       {
          auto account_record_itr = my->_accounts.find( account_itr->second );
          FC_ASSERT( account_record_itr != my->_accounts.end() );
          auto account_number = account_record_itr->second.account_number;

          // negative account_numbers are reserved for sending accounts
          FC_ASSERT( account_number >= 0, "private keys can only be imported for receive accounts" );
          my->import_private_key( priv_key, account_number, invoice_memo );
       }
       else
       {
          create_receive_account( account_name );
          import_private_key( priv_key, account_name, invoice_memo );
       }
   } FC_RETHROW_EXCEPTIONS( warn, "", ("account_name",account_name) ) }

   void detail::wallet_impl::import_private_key( const private_key_type& priv_key,
                                                 int32_t account_number,
                                                 const std::string& invoice_memo )
   { try {
      FC_ASSERT( self->is_unlocked() );
      auto key = priv_key.get_public_key();
      address pub_address( key );
      if( _receive_keys.find( pub_address ) != _receive_keys.end() )
      {
         wlog( "duplicate import of key ${a}", ("a",pub_address) );
         return;
      }
      int32_t payment_number = _extra_receive_keys.size();
      /// negative invoice numbers are reserved for private keys that are not derived from master
      int32_t invoice_number = -1; // TODO: derive invoice number from invoice_memo
      auto record_id = get_new_index();
      auto pkr = private_key_record( record_id, account_number, payment_number, priv_key, _wallet_password );
      elog( "account_number: ${i}", ("i",account_number) );
      _extra_receive_keys[payment_number] = pkr;

      address_index key_location( account_number, invoice_number, payment_number );

      _receive_keys[pub_address] = key_location;
      _receive_keys[ address(pts_address(key,false,56) )] = key_location;
      _receive_keys[ address(pts_address(key,true,56) ) ] = key_location;
      _receive_keys[ address(pts_address(key,false,0) ) ] = key_location;
      _receive_keys[ address(pts_address(key,true,0) )  ] = key_location;

      store_record( pkr );
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   void wallet::scan_state()
   { try {
      scan_balances();
      scan_assets();
      scan_names();
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   void wallet::scan_balances()
   { try {
      my->_blockchain->scan_balances( [=]( const balance_record& rec )
      {
          std::cout << std::string(rec.id()) << "  " << rec.balance << "\n";
          my->scan_balance( rec );
      });
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   void wallet::scan_assets()
   { try {
      my->_blockchain->scan_assets( [=]( const asset_record& rec )
      {
          my->scan_asset( rec );
      });
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   void wallet::scan_names()
   { try {
      my->_blockchain->scan_names( [=]( const name_record& rec )
      {
          my->scan_name( rec );
      });
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   /**
    *  @todo respect account name, for now it will ignore the account name
    */
   asset wallet::get_balance( const std::string& account_name, asset_id_type asset_id )
   { try {
      asset balance(0,asset_id);
      for( auto item : my->_balances )
         balance += item.second.get_balance( asset_id );
      return balance;
   } FC_RETHROW_EXCEPTIONS( warn, "", ("account_name",account_name)("asset_id",asset_id) ) }


   address  wallet::get_new_address( const std::string& account_name,
                                     uint32_t invoice_number )
   { try {
      return address( get_new_public_key( account_name, invoice_number ) );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("name",account_name) ) }

   public_key_type  wallet::get_new_public_key( const std::string& account_name,
                                                uint32_t invoice_number)
   { try {
      FC_ASSERT( is_unlocked() );
      auto account_name_itr = my->_account_name_index.find( account_name );
      if( account_name_itr != my->_account_name_index.end() )
      {
         auto account = my->get_account( account_name_itr->second );
         auto key_index  = account.get_next_key_index( invoice_number );
         wlog( "hindex: ${h}", ("h",key_index) );
         my->_accounts[ account_name_itr->second ] = account;
         my->store_record( account );

         auto priv_key = my->get_private_key( key_index );
         auto pub_key  = priv_key.get_public_key();
         address pub_addr(pub_key);
         my->_receive_keys[ pub_addr ] =key_index;

         return pub_key;
      }
      else
      {
         wlog( "create account for '${account_name}'",
               ("account_name",account_name) );

         create_receive_account( account_name );
         return get_new_public_key( account_name, invoice_number );
      }
   } FC_RETHROW_EXCEPTIONS( warn, "", ("account_name",account_name) ) }


   invoice_summary  wallet::transfer( const std::string& to_account_name,
                                      const asset& amount,
                                      const std::string& invoice_memo,
                                      const std::string& from_account_name,
                                      wallet_flag options )
   { try {
      FC_ASSERT( is_unlocked() );
      invoice_summary result;

      std::unordered_set<address> required_sigs;

      signed_transaction trx;
      if( amount.asset_id == my->_priority_fee.asset_id )
      {
         // TODO: limit to from_account_name
         my->withdraw_to_transaction( trx, amount + my->_priority_fee, required_sigs );
      }
      else
      {
         // TODO: limit to from_account_name
         my->withdraw_to_transaction( trx, amount, required_sigs );
         my->withdraw_to_transaction( trx, my->_priority_fee, required_sigs );
      }

      name_id_type delegate_id = rand()%BTS_BLOCKCHAIN_NUM_DELEGATES + 1;

      int32_t sending_invoice_index;
      int32_t last_sending_payment_index;
      address payment_address;
      my->get_new_payment_address_from_account( to_account_name, sending_invoice_index, last_sending_payment_index, payment_address );

      // get next payment_address for to_account_name

      trx.deposit( payment_address, amount, delegate_id );
      my->sign_transaction( trx, required_sigs );

      result.payments[trx.id()]         = trx;
      result.from_account               = from_account_name;
      result.to_account                 = to_account_name;
      result.sending_invoice_index      =
      result.last_sending_payment_index = 0;
      return result;
   } FC_RETHROW_EXCEPTIONS( warn, "", ("to_account",to_account_name)
                                  ("amount",amount)
                                  ("invoice_memo",invoice_memo)
                                  ("from_account",from_account_name)
                                  ("options",options) ) }

   /*
   signed_transaction wallet::send_to_address( const asset& amount,
                                               const address& owner,
                                               const std::string& invoice_memo )
   { try {

     return trx;
   } FC_RETHROW_EXCEPTIONS( warn, "", ("amount",amount)("owner",owner) ) }
   */


   signed_transaction wallet::update_name( const std::string& name,
                                           fc::optional<fc::variant> json_data,
                                           fc::optional<public_key_type> active,
                                           bool as_delegate,
                                           wallet_flag flag )
   { try {
      auto name_rec = my->_blockchain->get_name_record( name );
      FC_ASSERT( !!name_rec );

      std::unordered_set<address> required_sigs;
      signed_transaction trx;

      asset total_fees = my->_priority_fee;

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

   signed_transaction wallet::reserve_name( const std::string& name,
                                            const fc::variant& json_data,
                                            bool as_delegate,
                                            const std::string& account_name,
                                            wallet_flag flag )
   { try {
      FC_ASSERT( name_record::is_valid_name( name ), "", ("name",name) );
      auto name_rec = my->_blockchain->get_name_record( name );
      FC_ASSERT( !name_rec, "Name '${name}' has already been reserved", ("name",name)  );

      std::unordered_set<address> required_sigs;
      signed_transaction trx;

      asset total_fees = my->_priority_fee;

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

   bool wallet::is_receive_address( const address& addr )const
   {
      auto itr = my->_receive_keys.find( addr );
      if( itr != my->_receive_keys.end() )
      {
         my->cache_deterministic_keys( my->get_account( itr->second.account_number ), itr->second.invoice_number, itr->second.payment_number );
         return true;
      }
      return false;
   }

   bool wallet::is_sending_address( const address& addr )const
   {
      auto itr = my->_sending_keys.find( addr );
      return itr != my->_sending_keys.end();
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
      uint32_t interval_number = now.sec_since_epoch() / BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
      uint32_t round_start = (interval_number / BTS_BLOCKCHAIN_NUM_DELEGATES) * BTS_BLOCKCHAIN_NUM_DELEGATES;

      fc::time_point_sec next_time;

      auto sorted_delegates = my->_blockchain->get_delegates_by_vote();
      for( unsigned i = 0; i < sorted_delegates.size(); ++i )
      {
         auto name_itr = my->_names.find( sorted_delegates[i] );
         if( name_itr != my->_names.end() )
         {
             if( (round_start + i) * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC < now.sec_since_epoch() )
             {
                fc::time_point_sec tmp((round_start + i + BTS_BLOCKCHAIN_NUM_DELEGATES) * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC);
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
   void wallet::import_bitcoin_wallet( const fc::path& wallet_dat,
                                       const std::string& passphrase,
                                       const std::string& account_name,
                                       const std::string& invoice_memo )
   { try {
      auto priv_keys = bts::import_bitcoin_wallet(  wallet_dat, passphrase );
      for( auto key : priv_keys )
      {
         import_private_key( key, wallet_dat.filename().generic_string() );
      }
   } FC_RETHROW_EXCEPTIONS( warn, "Unable to import bitcoin wallet ${wallet_dat}", ("wallet_dat",wallet_dat) ) }

   void wallet::import_wif_private_key( const std::string& wif, 
                                const std::string& account_name, 
                                const std::string& invoice_memo )
   { try {
      auto wif_bytes = fc::from_base58(wif);
      auto key = fc::variant(std::vector<char>(wif_bytes.begin() + 1, wif_bytes.end() - 4)).as<fc::ecc::private_key>();
      auto check = fc::sha256::hash( wif_bytes.data(), wif_bytes.size() -4 );
      if( 0 == memcmp( (char*)&check, wif_bytes.data() + wif_bytes.size() -4, 4 ) )
         import_private_key(key, account_name, invoice_memo);
      else
         FC_ASSERT( !"Invalid Private Key Format" );
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
      ilog( "scan block by wallet ${wallet_name}", ("wallet_name",my->_wallet_name) );
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

   std::unordered_map<transaction_id_type,wallet_transaction_record>  wallet::transactions( const std::string& account_name )const
   {
      return my->_transactions;
   }

} } // bts::wallet
