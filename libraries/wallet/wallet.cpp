#include <bts/blockchain/config.hpp>
#include <bts/blockchain/time.hpp>
#include <bts/db/level_map.hpp>
#include <bts/import_bitcoin_wallet.hpp>
#include <bts/wallet/wallet.hpp>

#include <fc/crypto/aes.hpp>
#include <fc/crypto/base58.hpp>
#include <fc/io/raw.hpp>
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
            wallet_impl() : _is_open(false), _data_dir(".")
            {
               // This effectively sets the maximum transaction size to 10KB without
               // custom code. We will set this initial fee high while the network
               // is of lower value to prevent initial spam attacks. We also
               // want to subsidize the delegates early on.
               _priority_fee = asset( 1000*100, 0 );
            }

            virtual ~wallet_impl()override
            {
            }

            secret_hash_type get_secret( uint32_t block_num, const fc::ecc::private_key& delegate_key )
            {
               block_id_type header_id;
               if( block_num != uint32_t(-1) && block_num > 1 )
               {
                  auto block_header = _blockchain->get_block_header( block_num - 1 );
                  header_id = block_header.id();
               }

               fc::sha512::encoder key_enc;
               fc::raw::pack( key_enc, delegate_key );
               fc::sha512::encoder enc;
               fc::raw::pack( enc, key_enc.result() );
               fc::raw::pack( enc, header_id );

               return fc::ripemd160::hash( enc.result() );
            }

            void cache_deterministic_keys( const wallet_account_record& account, int32_t invoice_number, int32_t payment_number )
            {
               if( invoice_number < 0 )
                  return;
               if( account.account_number >= 0 )
               {
                     uint32_t last = payment_number + 2;
                     for( uint32_t i = 0; i <= last; ++i )
                     {
                        //ilog( "receive caching: ${a}.${i}.${p} as ${k}", ("a",account.account_number)("i",invoice_number)("p",i)("k",  address(account.get_key( invoice_number, i ))));
                        _receive_keys[ account.get_key( invoice_number, i ) ] = address_index( account.account_number, invoice_number, i );
                     }
               }
               else
               {
                     uint32_t last = payment_number + 2;
                     for( uint32_t i = 0; i <= last; ++i )
                     {
                        //ilog( "send caching: ${a}.${i}.${p} as ${k}", ("a",account.account_number)("i",invoice_number)("p",i)("k",  address(account.get_key( invoice_number, i ))));
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

            virtual void state_changed( const pending_chain_state_ptr& applied_changes )override
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
            virtual void block_applied( const block_summary& summary )override
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

            wallet*                                                             self;

            asset                                                               _priority_fee;

            bool                                                                _is_open;
            fc::time_point                                                      _relock_time;
            fc::future<void>                                                    _wallet_relocker_done;
            fc::path                                                            _data_dir;
            std::string                                                         _wallet_name;
            fc::path                                                            _wallet_filename;

            /** meta_record_property_enum is the key */
            std::unordered_map<int,wallet_meta_record>                          _meta;

            chain_database_ptr                                                  _blockchain;

            /** map record id to encrypted record data , this db should only be written to via
             *  my->store_record()
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
            std::unordered_map<address, address_index>                          _receive_keys;
            std::unordered_map<address, address_index>                          _sending_keys;

            /** stores the user's trust level for delegates */
            std::map<std::string, delegate_trust_status>                        _delegate_trust_status_map;

            /** used when address_index == (X,-1,N) to lookup foreign private keys, where
             * N is the key into _extra_receive_keys
             **/
            std::unordered_map<int32_t, private_key_record >                    _extra_receive_keys;

            std::unordered_map<std::string, uint32_t>                           _account_name_index;

            int32_t get_new_index()
            {
               int32_t new_index = 1;

               auto meta_itr = _meta.find( next_record_number );
               if( meta_itr == _meta.end() )
               {
                  _meta[next_record_number] = wallet_meta_record( new_index - 1, next_record_number, new_index + 1 );
               }
               else
               {
                  new_index = meta_itr->second.value.as<int32_t>();
                   _meta[next_record_number].value = new_index + 1;
               }

               FC_ASSERT( _meta[next_record_number].index == 0 );
               store_record( _meta[next_record_number] );

               return new_index;
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

            std::string get_address_label( const address_index& idx )
            {
               auto account_itr = _accounts.find( idx.account_number );
               if( account_itr != _accounts.end() )
                  return account_itr->second.name;
               return std::string();
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

            asset get_default_fee()
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
               FC_ASSERT( !password.empty() , "No wallet password specified" );

               auto key = fc::ecc::private_key::generate();
               auto chain_code = fc::ecc::private_key::generate();

               extended_private_key exp( key.get_secret(), chain_code.get_secret() );

               _master_key = master_key_record();
               _master_key->index = get_new_index();
               _master_key->encrypted_key = fc::aes_encrypt( _wallet_password, fc::raw::pack(exp) );
               /** TODO: salt the password checksum with something */
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
                switch( (withdraw_condition_types)account.condition.type )
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
                    //wlog( "not our name ${r}", ("r",name) );
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

            bool is_receive_address( const address& a ) 
            { 
               return self->is_receive_address( a ); 
            }

            bool scan_deposit( const deposit_operation& op )
            {
               switch( (withdraw_condition_types) op.condition.type )
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
               return _assets.find( op.amount.asset_id ) != _assets.end();
            }

            void scan_transaction( const signed_transaction& trx )
            {
               //ilog( "scan transaction ${wallet}  - ${trx}", ("wallet",_wallet_name)("trx",trx) );
                bool mine = false;
                for( auto op : trx.operations )
                {
                   switch( (operation_type_enum)op.type  )
                   {
                     case null_op_type:
                        break;
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
                     case fire_delegate_op_type:
                        // TODO
                        break;
                     case submit_proposal_op_type:
                        // TODO
                        break;
                     case vote_proposal_op_type:
                        // TODO
                        break;
                     default:
                        FC_ASSERT( false, "Transaction ${t} contains unknown operation type ${o}", ("t",trx)("o",op.type) );
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
                  auto rec = wallet_transaction_record( get_new_index(), trx );
                  auto loc = _blockchain->get_transaction_location( trx_id );
                  if( loc.valid() ) 
                  {
                      rec.location = *loc;
                      rec.received = _blockchain->get_block_header( loc->block_num ).timestamp;
                  }
                  _transactions[trx_id] = rec;
                  trx_rec_itr = _transactions.find( trx_id );
               }
               store_record( trx_rec_itr->second );
            }

            // TODO: optimize input balance collection
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

                         //ilog( "withdraw amount ${a} ${total_left}",
                          //     ("a",withdraw_amount)("total_left",total_left) );
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

            void load_records(const std::string& password)
            {
              for( auto record_itr = _wallet_db.begin(); record_itr.valid(); ++record_itr )
              {
                 auto record = record_itr.value();
                 switch( (wallet_record_type)record.type )
                 {
                    case master_key_record_type:
                    {
                       _master_key = record.as<master_key_record>();
                       self->unlock( fc::seconds( 60 * 5 ), password );
                       break;
                    }
                    case account_record_type:
                    {
                       auto cr = record.as<wallet_account_record>();
                       _accounts[cr.account_number] = cr;
                       _account_name_index[cr.name] = cr.account_number;

                       cache_deterministic_keys( cr );
                       break;
                    }
                    case transaction_record_type:
                    {
                       auto wtr    = record.as<wallet_transaction_record>();
                       auto trx_id = wtr.trx.id();
                       _transactions[trx_id] = wtr;
                       break;
                    }
                    case name_record_type:
                    {
                       auto wnr = record.as<wallet_name_record>();
                       _names[wnr.id] = wnr;
                       break;
                    }
                    case asset_record_type:
                    {
                       auto wnr = record.as<wallet_asset_record>();
                       _assets[wnr.id] = wnr;
                       break;
                    }
                    case balance_record_type:
                    {
                       auto war = record.as<wallet_balance_record>();
                       _balances[war.id()] = war;
                       break;
                    }
                    case private_key_record_type:
                    {
                       auto pkr = record.as<private_key_record>();
                       _extra_receive_keys[pkr.extra_key_index] = pkr;
                       auto pubkey = pkr.get_private_key(_wallet_password).get_public_key();
                       elog( "public key: ${key}", ("key",pubkey) );
                       _receive_keys[ address( pubkey ) ] =
                          address_index( pkr.account_number, -1, pkr.extra_key_index );
                       _receive_keys[ address(pts_address(pubkey,false,56) )] =
                          address_index( pkr.account_number, -1, pkr.extra_key_index );
                       _receive_keys[ address(pts_address(pubkey,true,56) ) ] =
                          address_index( pkr.account_number, -1, pkr.extra_key_index );
                       _receive_keys[ address(pts_address(pubkey,false,0) ) ] =
                          address_index( pkr.account_number, -1, pkr.extra_key_index );
                       _receive_keys[ address(pts_address(pubkey,true,0) )  ] =
                          address_index( pkr.account_number, -1, pkr.extra_key_index );
                       break;
                    }
                    case meta_record_type:
                    {
                       auto metar = record.as<wallet_meta_record>();
                       _meta[metar.key] = metar;
                       break;
                    }
                    default:
                    {
                       FC_ASSERT( false, "Wallet database contains unknown record type ${type}", ("type",record.type) );
                       break;
                    }
                 }
              }

              self->lock();
            }

      }; // wallet_impl

   } // namespace detail

   wallet::wallet( const chain_database_ptr& chain_db )
   :my( new detail::wallet_impl() )
   {
      my->self = this;
      my->_blockchain = chain_db;
      chain_db->set_observer( my.get() );
   }

   wallet::~wallet()
   { 
      try 
      { 
        close(); 
      }
      catch( ... )
      {
      } 
   }

   void wallet::create( const std::string& wallet_name, const std::string& password )
   { try {
      FC_ASSERT( !wallet_name.empty() );
      FC_ASSERT( !password.empty() );
      auto wallet_filename = my->_data_dir / wallet_name;
      FC_ASSERT( !fc::exists( wallet_filename ), "Wallet ${wallet_filename} already exists.", ("wallet_filename",wallet_filename) )
      close();

      try
      {
          my->_wallet_db.open( wallet_filename, true );
          my->initialize_wallet( password );
          open( wallet_name, password );
      }
      catch( ... )
      {
          close();
          fc::remove_all( wallet_filename );
          throw;
      }
   } FC_RETHROW_EXCEPTIONS( warn, "unable to create wallet ${wallet_name}", ("wallet_name",wallet_name) ) }

   void wallet::open( const std::string& wallet_name, const std::string& password )
   { try {
      FC_ASSERT( !wallet_name.empty() );
      FC_ASSERT( !password.empty() );

      try
      {
          open_file( my->_data_dir / wallet_name, password );
          my->_wallet_name = wallet_name;
      }
      catch( ... )
      {
          close();
          throw;
      }
   } FC_RETHROW_EXCEPTIONS( warn, "", ("wallet_name",wallet_name) ) }

   void wallet::open_file( const fc::path& wallet_filename, const std::string& password )
   { try {
      FC_ASSERT( !password.empty() );
      FC_ASSERT( fc::exists( wallet_filename ), "Unable to open ${wallet_filename}", ("wallet_filename",wallet_filename) );
      close();

      try
      {
          my->_wallet_db.open( wallet_filename, true );
          my->load_records( password );

          FC_ASSERT( !!my->_master_key, "No master key found in wallet" )
          my->_wallet_filename = wallet_filename;

          my->_priority_fee = my->get_default_fee();
          scan_chain( my->get_last_scanned_block_number() );

          my->_is_open = true;
      }
      catch( ... )
      {
          close();
          throw;
      }
   } FC_RETHROW_EXCEPTIONS( warn, "unable to open wallet '${file}'", ("file",wallet_filename) ) }

   bool wallet::close()
   { try {
      if( my->_wallet_relocker_done.valid() )
      {
         my->_wallet_relocker_done.cancel();
         if( my->_wallet_relocker_done.ready() ) // TODO: There may be a deeper issue here; see unlock()
           my->_wallet_relocker_done.wait();
      }
      my->_relock_time = fc::time_point();

      my->_wallet_name = "";
      my->_wallet_filename = fc::path();

      my->_meta.clear();
      my->_wallet_db.close();

      my->_wallet_password = fc::sha512();
      my->_master_key.reset();

      my->_accounts.clear();
      my->_balances.clear();
      my->_names.clear();
      my->_assets.clear();
      my->_transactions.clear();
      my->_receive_keys.clear();
      my->_sending_keys.clear();
      my->_delegate_trust_status_map.clear();
      my->_extra_receive_keys.clear();
      my->_account_name_index.clear();

      my->_is_open = false;
      return true;
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   void wallet::export_to_json( const fc::path& path )
   {
       FC_ASSERT( !fc::exists( path ) );
       FC_ASSERT( is_open() );
       std::map< uint32_t, wallet_record > db_map;

       for( auto iter = my->_wallet_db.begin(); iter.valid(); ++iter )
           db_map[ iter.key() ] = iter.value();

       fc::json::save_to_file( db_map, path, true );
   }

   void wallet::create_from_json( const fc::path& path, const std::string& name, const std::string& passphrase )
   {
       FC_ASSERT( fc::exists( path ) );
       auto db_map = fc::json::from_file< std::map< uint32_t, wallet_record > >( path );

       create( name, passphrase );

       try
       {
           for( auto item : db_map )
               my->_wallet_db.store( item.first, item.second );

           my->load_records( passphrase );
           scan_chain( my->get_last_scanned_block_number() );
       }
       catch( ... )
       {
          close();
          fc::remove_all( my->_data_dir / name );
          throw;
       }
   }

   bool wallet::is_open()const                   { return my->_is_open;                         }
   std::string wallet::get_name()const           { return my->_wallet_name;                     }
   fc::path    wallet::get_filename()const       { return my->_wallet_filename;                 }

   void wallet::lock()                           
   { 
      wlog( "lock ${this}", ("this",int64_t(this)) );
      my->_wallet_password = fc::sha512(); 
      my->_relock_time = fc::time_point();  
   }
   bool wallet::is_unlocked()const               { return my->_wallet_password != fc::sha512(); }
   bool wallet::is_locked()const                 { return !is_unlocked();                       }
   fc::time_point wallet::unlocked_until() const { return my->_relock_time; }

   /**
    * TODO
    * @todo remove sleep/wait loop for duration and use scheduled notifications to relock
    */
   void wallet::unlock( const fc::microseconds& timeout, const std::string& password )
   { try {
      wlog( "unlock: ${sec}  ${this}", ("sec",timeout.to_seconds())("this",int64_t(this)) );
      FC_ASSERT( timeout.count() > 0 );
      FC_ASSERT( !password.empty() );
      FC_ASSERT( !!my->_master_key );
      my->_wallet_password = fc::sha512::hash( password.c_str(), password.size() );

      /** TODO: salt the password checksum with something */
      if( my->_master_key->checksum != fc::sha512::hash( my->_wallet_password ) )
      {
         my->_wallet_password = fc::sha512();
         FC_THROW("Incorrect passphrase");
      }
      if( timeout == fc::microseconds::maximum() )
      {
         my->_relock_time = fc::time_point::maximum();
      }
      else
      {
         fc::time_point requested_relocking_time = fc::time_point::now() + timeout;
         my->_relock_time = std::max(my->_relock_time, requested_relocking_time);
         if (!my->_wallet_relocker_done.valid() || my->_wallet_relocker_done.ready())
         {
           my->_wallet_relocker_done = fc::async([this](){
             while( !my->_wallet_relocker_done.canceled() )
             {
               if (bts::blockchain::now() > my->_relock_time)
               {
                 lock();
                 return;
               }
               fc::usleep(fc::microseconds(200000));
             }
           });
         }
      }
#if 0
      // change the above code to this version once task cancellation is fixed in fc
      // if we're currently unlocked and have a timer counting down,
      // kill it and start a new one
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
      FC_ASSERT( !new_password.empty() );

      auto new_wallet_password = fc::sha512::hash( new_password.c_str(), new_password.size() );

      // iterate over all private key records and re-encrypt them
      for ( auto priv_key_record : my->_extra_receive_keys)
      {
         auto private_key = priv_key_record.second.get_private_key(my->_wallet_password);
         priv_key_record.second.encrypted_key = fc::aes_encrypt( new_wallet_password, fc::raw::pack( private_key ) );

         my->store_record(priv_key_record.second);
      }

      // re-encrypt master private
      auto master_private_key = my->_master_key->get_extended_private_key(my->_wallet_password);
      my->_master_key->encrypted_key = fc::aes_encrypt( new_wallet_password, fc::raw::pack(master_private_key) );
      /** TODO: salt the password checksum with something */
      my->_master_key->checksum = fc::sha512::hash( new_wallet_password );
      my->store_record( *my->_master_key  );

      my->_wallet_password = new_wallet_password;
   }

   wallet_account_record wallet::create_receive_account( const std::string& account_name )
   { try {
        if (is_locked())
          FC_ASSERT(!"wallet must be unlocked to create receive account", "", 
                    ("account_name",account_name)("this",int64_t(this))("name",get_name()) );

        auto current_itr = my->_account_name_index.find( account_name );
        if (current_itr != my->_account_name_index.end())
          FC_THROW_EXCEPTION(invalid_arg_exception, "Account name '${name}' already in use", ("name", account_name));

        wallet_account_record wcr;
        wcr.index             = my->get_new_index();
        wcr.account_number    = my->get_next_account_number();
        wcr.name              = account_name;

        auto master_key = my->_master_key->get_extended_private_key(my->_wallet_password);
        wcr.extended_key = master_key.child( wcr.account_number );

        my->_account_name_index[account_name] = wcr.account_number;
        my->_accounts[wcr.account_number] = wcr;

        my->store_record( wcr );
        my->cache_deterministic_keys( wcr, 0, 0 );
        my->cache_deterministic_keys( wcr, 1, 0 );
        return wcr;
   } FC_RETHROW_EXCEPTIONS( warn, "unable to create account", ("account_name",account_name) ) }

   void wallet::rename_account( const std::string& current_account_name,
                                const std::string& new_account_name )
   { try {
        FC_ASSERT( current_account_name != new_account_name );
        FC_ASSERT( new_account_name != "*" );

        auto current_index_itr = my->_account_name_index.find(current_account_name);
        if( current_index_itr == my->_account_name_index.end() )
           FC_THROW_EXCEPTION( invalid_arg_exception, "Invalid account name '${name}'", ("name",current_account_name) );

        auto new_index_itr = my->_account_name_index.find(new_account_name);
        if( new_index_itr != my->_account_name_index.end() )
           FC_THROW_EXCEPTION( invalid_arg_exception, "Account name '${name}' already in use", ("name",new_account_name) );

       my->_accounts[current_index_itr->second].name = new_account_name;
       my->store_record( my->_accounts[current_index_itr->second] );

       my->_account_name_index[ new_account_name ] = current_index_itr->second;
       my->_account_name_index.erase( current_index_itr );

   } FC_RETHROW_EXCEPTIONS( warn, "Error renaming account",
                            ("current_account_name",current_account_name)
                            ("new_account_name",new_account_name) ) }

   void wallet::create_sending_account( const std::string& account_name,
                                        const extended_public_key& account_pub_key )
   { try {
        auto current_itr = my->_account_name_index.find(account_name);
        if (current_itr != my->_account_name_index.end())
           FC_THROW_EXCEPTION( invalid_arg_exception, "Account name '${name}' already in use", ("name",account_name) );

        wallet_account_record wcr;
        wcr.index             =  my->get_new_index();
        wcr.account_number    = -my->get_next_account_number();
        wcr.name              =  account_name;

        wcr.extended_key = account_pub_key;
        //wlog( "creating account '${account_name}'", ("account_name",wcr) );

        my->_account_name_index[account_name] = wcr.account_number;
        my->_accounts[wcr.account_number] = wcr;

        my->store_record( wcr );
        my->cache_deterministic_keys( wcr, 0, 0 );
        my->cache_deterministic_keys( wcr, 1, 0 );
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
         FC_THROW_EXCEPTION( invalid_arg_exception, "Invalid account name '${name}'", ("name",account_name) );
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
      ilog( "account_number: ${i}", ("i",account_number) );
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
          // std::cout << std::string(rec.id()) << "  " << rec.balance << "\n";
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

   // TODO: save memo
   invoice_summary  wallet::transfer( const std::string& to_account_name,
                                      const asset& amount,
                                      const std::string& from_account_name,
                                      const std::string& invoice_memo,
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
         // TODO: optimize fee calculation
         my->withdraw_to_transaction( trx, amount, required_sigs );
         my->withdraw_to_transaction( trx, my->_priority_fee, required_sigs );
      }

      // TODO: implement wallet voting algorithm
      name_id_type delegate_id = rand()%BTS_BLOCKCHAIN_NUM_DELEGATES + 1;

      // get next payment_address for to_account_name
      int32_t sending_invoice_index;
      int32_t last_sending_payment_index;
      address payment_address;
      my->get_new_payment_address_from_account( to_account_name, sending_invoice_index, last_sending_payment_index, payment_address );

      trx.deposit( payment_address, amount, delegate_id );
      my->sign_transaction( trx, required_sigs );

      result.payments[trx.id()]         = trx;
      result.from_account               = from_account_name;
      result.to_account                 = to_account_name;
      result.sending_invoice_index      = sending_invoice_index;
      result.last_sending_payment_index = last_sending_payment_index;
      return result;
   } FC_RETHROW_EXCEPTIONS( warn, "", ("to_account",to_account_name)
                                  ("amount",amount)
                                  ("invoice_memo",invoice_memo)
                                  ("from_account",from_account_name)
                                  ("options",options) ) }


   signed_transaction  wallet::create_asset( const std::string& symbol,
                                                const std::string& asset_name,
                                                const std::string& description,
                                                const fc::variant& data,
                                                const std::string& issuer_name,
                                                share_type max_share_supply,
                                                const std::string& account_name,
                                                wallet_flag options  )
   { try {
      FC_ASSERT( is_unlocked() );
      std::unordered_set<address> required_sigs;
      signed_transaction trx;

      oasset_record prior_asset = my->_blockchain->get_asset_record( symbol );
      FC_ASSERT( !prior_asset, "Asset already registered", ("prior_asset",*prior_asset) );

      oname_record issuer_record = my->_blockchain->get_name_record( issuer_name );
      FC_ASSERT( prior_asset, "Issuer must be a registered name", ("name",issuer_name) );

      required_sigs.insert( address( issuer_record->active_key ) );

      trx.create_asset( symbol, asset_name, description, data, issuer_record->id, max_share_supply );

      size_t trx_size = fc::raw::pack_size( trx );
      size_t fees = trx_size + BTS_BLOCKCHAIN_ASSET_REGISTRATION_FEE;
      fees       *= my->_blockchain->get_fee_rate();

      my->withdraw_to_transaction( trx, fees, required_sigs );
      my->sign_transaction( trx, required_sigs );

      return trx;
   } FC_RETHROW_EXCEPTIONS( warn, "Unable to create asset ${symbol}", ("symbol",symbol)("asset_name",asset_name) ) }


   signed_transaction  wallet::issue_asset( const std::string& symbol, share_type amount,
                                               const std::string& to_account_name )
   { try {
      FC_ASSERT( is_unlocked() );
      FC_ASSERT( amount > 0 );

      std::unordered_set<address> required_sigs;
      signed_transaction trx;

      oasset_record asset_to_issue = my->_blockchain->get_asset_record( symbol );
      FC_ASSERT( asset_to_issue.valid(), "Unknown Asset '${symbol}'", ("symbol",symbol) );
      FC_ASSERT( asset_to_issue->issuer_name_id == asset_record::market_issued_asset, "Cannot issue market-issued asset" );

      oname_record issuer_record = my->_blockchain->get_name_record( asset_to_issue->issuer_name_id );
      FC_ASSERT( issuer_record, "Database is corrupted, there should be a registered issuer" );
      required_sigs.insert( address( issuer_record->active_key ) );

      FC_ASSERT( asset_to_issue->can_issue( amount ) );

      trx.issue( asset( amount, asset_to_issue->id ) );

      int32_t sending_invoice_index;
      int32_t last_sending_payment_index;
      address payment_address;
      my->get_new_payment_address_from_account( to_account_name, 
                                                sending_invoice_index, 
                                                last_sending_payment_index, 
                                                payment_address );

      size_t trx_size = fc::raw::pack_size( trx );
      size_t fees = trx_size + BTS_BLOCKCHAIN_ASSET_REGISTRATION_FEE;
      fees       *= my->_blockchain->get_fee_rate();

      my->withdraw_to_transaction( trx, fees, required_sigs );

      my->sign_transaction( trx, required_sigs );
      return trx;
   } FC_RETHROW_EXCEPTIONS( warn, "", ("symbol",symbol)("amount",amount)("account_name",to_account_name) ) }



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


      if( json_data.valid() )
      {
         size_t pack_size = fc::raw::pack_size(*json_data);
         FC_ASSERT( pack_size < BTS_BLOCKCHAIN_MAX_NAME_DATA_SIZE );
         total_fees += asset( (pack_size * current_fee_rate)/1000, 0 );
      }
      if( !as_delegate && name_rec->is_delegate() )
         FC_ASSERT( !"You cannot unregister as a delegate" );

      if( !name_rec->is_delegate() && as_delegate )
      {
        total_fees += asset( (BTS_BLOCKCHAIN_DELEGATE_REGISTRATION_FEE*current_fee_rate)/1000, 0 );
      }

      my->withdraw_to_transaction( trx, total_fees, required_sigs );
      trx.update_name( name_rec->id, json_data, active, as_delegate );
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
      auto data_size = fc::raw::pack_size(json_data); //fc::json::to_string(json_data);
      total_fees += asset( (data_size * current_fee_rate)/1000, 0 );

      my->withdraw_to_transaction( trx, total_fees, required_sigs );

      auto owner_key  = get_new_public_key();
      auto active_key = get_new_public_key();
      trx.reserve_name( name, json_data, owner_key, active_key, as_delegate );

      my->sign_transaction( trx, required_sigs );

      return trx;
   } FC_RETHROW_EXCEPTIONS( warn, "", ("name",name)("data",json_data)("delegate",as_delegate) ) }

   signed_transaction wallet::submit_proposal( const std::string& name,
                                               const std::string& subject,
                                               const std::string& body,
                                               const std::string& proposal_type,
                                               const fc::variant& json_data,
                                               wallet_flag /* flag */)
   {
     try {
       auto name_rec = my->_blockchain->get_name_record(name);
       FC_ASSERT(!!name_rec);
       //TODO verify we are delegate?

       std::unordered_set<address> required_sigs;
       signed_transaction trx;

       //sign as "name"
       required_sigs.insert(address(name_rec->active_key));

       asset total_fees = my->_priority_fee;
       auto current_fee_rate = my->_blockchain->get_fee_rate();
       auto data_size = fc::raw::pack_size(json_data); //fc::json::to_string(json_data);
       total_fees += asset((data_size * current_fee_rate) / 1000, 0);
       my->withdraw_to_transaction(trx, total_fees, required_sigs);

       name_id_type delegate_id = name_rec->id;
       trx.submit_proposal(delegate_id, subject, body, proposal_type, json_data);

       my->sign_transaction(trx, required_sigs);
       return trx;
       //TODO fix rethrow
     } FC_RETHROW_EXCEPTIONS(warn, "", ("name", name)("subject", subject))
   }

   signed_transaction wallet::vote_proposal(const std::string& name, 
                                            proposal_id_type proposal_id,
                                            uint8_t vote,
                                            wallet_flag /* flag */)
   {
     try {
       auto name_rec = my->_blockchain->get_name_record(name);
       FC_ASSERT(!!name_rec);
       //TODO verify we are delegate?

       std::unordered_set<address> required_sigs;
       signed_transaction trx;

       //sign as "name"
       required_sigs.insert(address(name_rec->active_key));

       asset total_fees = my->_priority_fee;
       my->withdraw_to_transaction(trx, total_fees, required_sigs);

       name_id_type voter_id = name_rec->id;
       trx.vote_proposal(voter_id, proposal_id, vote);

       my->sign_transaction(trx, required_sigs);
       return trx;
       //TODO fix rethrow
     } FC_RETHROW_EXCEPTIONS(warn, "", ("name", name)("proposal_id", proposal_id)("vote", vote))
   }

   //functions for reporting delegate trust status
   void wallet::set_delegate_trust_status( const std::string& delegate_name, fc::optional<int32_t> trust_level )
   {
     my->_delegate_trust_status_map[delegate_name] = delegate_trust_status{ trust_level };
   }

   delegate_trust_status wallet::get_delegate_trust_status( const std::string& delegate_name ) const
   {
     return my->_delegate_trust_status_map[delegate_name];
   }

   std::map<std::string, delegate_trust_status> wallet::list_delegate_trust_status() const
   {
     return my->_delegate_trust_status_map;
   }

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
      auto signing_delegate_id = my->_blockchain->get_signing_delegate_id( header.timestamp );
      auto delegate_rec = my->_blockchain->get_name_record( signing_delegate_id );

      auto delegate_pub_key = my->_blockchain->get_signing_delegate_key( header.timestamp );
      auto delegate_key = my->get_private_key( delegate_pub_key );
      FC_ASSERT( delegate_pub_key == delegate_key.get_public_key() );

      header.previous_secret = my->get_secret( 
                                      delegate_rec->delegate_info->last_block_num_produced,
                                      delegate_key );

      auto next_secret = my->get_secret( my->_blockchain->get_head_block_num() + 1, delegate_key );
      header.next_secret_hash = fc::ripemd160::hash( next_secret );

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
      auto now = bts::blockchain::now();
      uint32_t interval_number = now.sec_since_epoch() / BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
      if( now == my->_blockchain->get_head_block().timestamp ) interval_number++;
      uint32_t next_block_time = interval_number * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
      uint32_t last_block_time = next_block_time + BTS_BLOCKCHAIN_NUM_DELEGATES * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;

      while( next_block_time < last_block_time )
      {
         auto id = my->_blockchain->get_signing_delegate_id( fc::time_point_sec( next_block_time ) );
         auto name_itr = my->_names.find( id );
         if( name_itr != my->_names.end() )
         {
            wlog( "next block time:  ${t}", ("t",fc::time_point_sec( next_block_time ) ) );
            return fc::time_point_sec( next_block_time ); 
         }
         next_block_time += BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
      }
      return fc::time_point_sec();
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
      auto check = fc::sha256::hash( wif_bytes.data(), wif_bytes.size() - 4 );
      if( 0 == memcmp( (char*)&check, wif_bytes.data() + wif_bytes.size() - 4, 4 ) )
         import_private_key(key, account_name, invoice_memo);
      else
         FC_ASSERT( !"Invalid Private Key Format" );
   } FC_RETHROW_EXCEPTIONS( warn, "unable to import wif private key" ) }

   void wallet::scan_chain( uint32_t block_num, scan_progress_callback cb  )
   { try {
        uint32_t head_block_num = my->_blockchain->get_head_block_num();
        if (block_num == 0)
        {
          scan_state();
          block_num = 1; // continue the scan from the first block, if it exists
        }
        for( uint32_t i = block_num; i <= head_block_num; ++i )
        {
            auto blk = my->_blockchain->get_block( i );
            scan_block( blk );
            if (cb)
              cb( i, head_block_num, 0, 0 );
            my->set_last_scanned_block_number( i );
        }
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   void wallet::scan_block( const full_block& blk )
   {
      //ilog( "scan block by wallet ${wallet_name}", ("wallet_name",my->_wallet_name) );
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

   fc::path wallet::get_data_directory()const
   {
      return my->_data_dir;
   }

   /**
    * TODO
    * @todo actually filter based upon account_name and use "*" to represent all accounts
    */
   std::vector<wallet_transaction_record> wallet::get_transactions( unsigned count )const
   {
       std::vector<wallet_transaction_record> trx_records;
       trx_records.reserve(my->_transactions.size());

       for( auto item : my->_transactions )
           trx_records.push_back( item.second );

       /* Sort from oldest to newest */
       auto comp = [](const wallet_transaction_record& a, const wallet_transaction_record& b)->bool
       {
           if (a.location.block_num == b.location.block_num) return a.location.trx_num < b.location.trx_num;
           return a.location.block_num < b.location.block_num;
       };
       std::sort(trx_records.begin(), trx_records.end(), comp);

       if (count > 0 && count < trx_records.size())
       return std::vector<wallet_transaction_record>(trx_records.end() - count, trx_records.end());

       return trx_records;
   }

   fc::optional<address> wallet::get_owning_address( const balance_id_type& id )const
   {
       auto itr = my->_balances.find( id );
       if( itr == my->_balances.end() )
           return fc::optional<address>();

       auto condition = itr->second.condition;
       if( withdraw_condition_types(condition.type) != withdraw_signature_type )
           return fc::optional<address>();

       return fc::optional<address>(condition.as<withdraw_with_signature>().owner);
   }

   fc::optional<wallet_account_record> wallet::get_account_record( const address& addr)const
   {
       auto itr = my->_receive_keys.find( addr );
       if( itr == my->_receive_keys.end() )
       {
           itr = my->_sending_keys.find( addr );
           if( itr == my->_sending_keys.end() )
               return fc::optional<wallet_account_record>();
       }

       if( my->_accounts.count( itr->second.account_number ) <= 0 )
           return fc::optional<wallet_account_record>();

       return fc::optional<wallet_account_record>( my->_accounts[itr->second.account_number] );
   }

   std::unordered_map<transaction_id_type,wallet_transaction_record>  wallet::transactions( const std::string& account_name )const
   {
       return my->_transactions;
   }

   /**
    *  @todo actually filter based upon account_name and use "*" to represetn all accounts
    */
   std::unordered_map<name_id_type, wallet_name_record>  wallet::names( const std::string& account_name  )const
   {
       return my->_names;
   }

} } // bts::wallet
