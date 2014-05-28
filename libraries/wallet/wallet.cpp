#include <bts/wallet/wallet.hpp>
#include <bts/wallet/wallet_db.hpp>
#include <bts/wallet/config.hpp>
#include <bts/blockchain/time.hpp>
#include <fc/thread/thread.hpp>
#include <fc/crypto/base58.hpp>
#include <fc/filesystem.hpp>
#include <fc/time.hpp>
#include <fc/variant.hpp>

namespace bts { namespace wallet {

   namespace detail {

      class wallet_impl
      {
         public:
             wallet_db          _wallet_db;
             chain_database_ptr _blockchain;
             fc::path           _data_directory;
             fc::path           _current_wallet_path;
             fc::time_point     _scheduled_lock_time;
             fc::future<void>   _wallet_relocker_done;
             fc::sha512         _wallet_password;


             secret_hash_type get_secret( uint32_t block_num,
                                          const fc::ecc::private_key& delegate_key )const;

             void scan_block( uint32_t block_num, 
                              const std::vector<fc::ecc::private_key>& keys );

             bool scan_withdraw( const withdraw_operation& op );

             bool scan_deposit( const deposit_operation& op, 
                                 const private_keys& keys );
             void cache_balance( const balance_id_type& balance_id );
      };

      secret_hash_type wallet_impl::get_secret( uint32_t block_num, 
                                                const fc::ecc::private_key& delegate_key )const
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

      void wallet_impl::scan_block( uint32_t block_num, 
                                    const private_keys& keys )
      {
         auto current_block = _blockchain->get_block( block_num );
         for( auto trx : current_block.user_transactions )
         {
            bool cache_trx = false;
            for( auto op : trx.operations )
            {
               switch( (operation_type_enum)op.type )
               {
                  case withdraw_op_type:
                     cache_trx |= scan_withdraw( op.as<withdraw_operation>() );
                     break;
                  case deposit_op_type:
                     cache_trx |= scan_deposit( op.as<deposit_operation>(), keys );
                     break;
               }
            }
            if( cache_trx )
               _wallet_db.store_transaction( trx );
         }
      }
      bool wallet_impl::scan_withdraw( const withdraw_operation& op )
      {
         auto current_balance = _wallet_db.lookup_balance( op.balance_id );
         if( current_balance.valid() )
            cache_balance( op.balance_id );
         return current_balance.valid();
      }

      bool wallet_impl::scan_deposit( const deposit_operation& op, 
                                      const private_keys& keys )
      {
          bool cache_deposit = false; 
          switch( (withdraw_condition_types) op.condition.type )
          {
             case withdraw_signature_type:
                cache_deposit |= _wallet_db.has_private_key( op.condition.as<withdraw_with_signature>().owner );
                break;
             case withdraw_by_account_type:
             {
                for( auto key : keys )
                {
                   auto deposit = op.condition.as<withdraw_by_account>();
                   omemo_status status = deposit.decrypt_memo_data( key );
                   if( status.valid() )
                   {
                      _wallet_db.cache_memo( *status, key, _wallet_password );
                      cache_deposit = true;
                      break;
                   }
                }
                break;
             }
             // TODO: support other withdraw types here..
          }
          if( cache_deposit )
             cache_balance( op.balance_id() );
          return cache_deposit;
      } // wallet_impl::scan_deposit 

      void wallet_impl::cache_balance( const balance_id_type& balance_id )
      {
         auto bal_rec = _blockchain->get_balance_record( balance_id );
         if( !bal_rec.valid() )
         {
            wlog( "blockchain doesn't know about balance id: ${balance_id}",
                  ("balance_id",balance_id) );
         }
         else
         {
            _wallet_db.cache_balance( *bal_rec );
         }
      }


   } // detail 

   wallet::wallet( chain_database_ptr blockchain )
   :my( new detail::wallet_impl() )
   {
      my->_blockchain = blockchain;
   }

   wallet::~wallet()
   {
      close();
   }

   void           wallet::set_data_directory( const fc::path& data_dir )
   {
      my->_data_directory = data_dir;
   }

   fc::path       wallet::get_data_directory()const
   {
      return my->_data_directory;
   }

   void wallet::create( const std::string& wallet_name, 
                        const std::string& password,
                        const std::string& brainkey  )
   { try {
      create_file( fc::absolute(my->_data_directory) / wallet_name, password, brainkey ); 
   } FC_RETHROW_EXCEPTIONS( warn, "Unable to create wallet '${name}' in ${data_dir}", 
                            ("wallet_name",wallet_name)("data_dir",fc::absolute(my->_data_directory)) ) }


   void wallet::create_file( const fc::path& wallet_file_path,
                        const std::string& password,
                        const std::string& brainkey  )
   { try {
      FC_ASSERT( !fc::exists( wallet_file_path ) );
      FC_ASSERT( password.size() > 8 );
      my->_wallet_db.open( wallet_file_path );
      
      auto password_hash = fc::sha512::hash( password.c_str(), password.size() );

      master_key new_master_key;
      if( brainkey.size() )
      {
         auto base = fc::sha512::hash( brainkey.c_str(), brainkey.size() );

         /* strengthen the key a bit */
         for( uint32_t i = 0; i < 100ll*1000ll; ++i )
            base = fc::sha512::hash( base );

         new_master_key.encrypt_key( password_hash, extended_private_key( base ) );
      }
      else
      {
         extended_private_key epk( fc::ecc::private_key::generate() );
         new_master_key.encrypt_key( password_hash, epk );
      }

      my->_wallet_db.store_record( wallet_master_key_record( new_master_key ) );

      my->_wallet_db.close();
      my->_wallet_db.open( wallet_file_path );

      FC_ASSERT( my->_wallet_db.wallet_master_key.valid() );

   } FC_RETHROW_EXCEPTIONS( warn, "Unable to create wallet '${wallet_file_path}'", 
                            ("wallet_file_path",wallet_file_path) ) }


   void wallet::open( const std::string& wallet_name )
   { try {
      open_file( get_data_directory() / wallet_name );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("wallet_name",wallet_name) ) }
   

   void wallet::open_file( const fc::path& wallet_filename )
   { 
      try {
         close();
         my->_wallet_db.open( wallet_filename );
         my->_current_wallet_path = wallet_filename;
      } FC_RETHROW_EXCEPTIONS( warn, 
             "Unable to open wallet ${filename}", 
             ("filename",wallet_filename) ) 
   }

   void wallet::close()
   {
      my->_wallet_db.close();
      if( my->_wallet_relocker_done.valid() )
      {
         lock();
         my->_wallet_relocker_done.cancel();
         if( my->_wallet_relocker_done.ready() ) 
           my->_wallet_relocker_done.wait();
      }
   }

   std::string wallet::get_wallet_name()const
   {
      return my->_current_wallet_path.filename().generic_string();
   }

   fc::path wallet::get_wallet_filename()const
   {
      return my->_current_wallet_path;
   }

   bool wallet::is_open()const
   {
      return my->_wallet_db.is_open();
   }

   void wallet::export_to_json( const fc::path& export_file_name ) const
   {
      my->_wallet_db.export_to_json( export_file_name );
   }

   void wallet::unlock( const fc::microseconds& timeout, const std::string& password )
   { try {
      FC_ASSERT( password.size() > BTS_MIN_PASSWORD_LENGTH ) 
      FC_ASSERT( timeout >= fc::seconds(1) );
      FC_ASSERT( my->_wallet_db.wallet_master_key.valid() );

      my->_wallet_password = fc::sha512::hash( password.c_str(), password.size() );
      if( !my->_wallet_db.wallet_master_key->validate_password( my->_wallet_password ) )
      {
         lock();
         FC_ASSERT( !"Invalid Password" );
      }

      if( timeout == fc::microseconds::maximum() )
      {
         my->_scheduled_lock_time = fc::time_point::maximum();
      }
      else
      {
         my->_scheduled_lock_time = fc::time_point::now() + timeout;
         if( !my->_wallet_relocker_done.valid() || my->_wallet_relocker_done.ready() )
         {
           my->_wallet_relocker_done = fc::async([this](){
             while( !my->_wallet_relocker_done.canceled() )
             {
               if (fc::time_point::now() > my->_scheduled_lock_time)
               {
                 lock();
                 return;
               }
               fc::usleep(fc::microseconds(200000));
             }
           });
         }
      }

   } FC_RETHROW_EXCEPTIONS( warn, "", ("timeout_sec", timeout.count()/1000000 ) ) }

   void wallet::lock()
   {
      my->_wallet_password     = fc::sha512();
      my->_scheduled_lock_time = fc::time_point();
      my->_wallet_relocker_done.cancel();
   }
   void wallet::change_passphrase( const std::string& new_passphrase )
   {
      FC_ASSERT( is_open() );
      FC_ASSERT( is_unlocked() );
      FC_ASSERT( !"TODO - Implement CHange Passphrase" );
   }

   bool wallet::is_unlocked()const
   {
      return !wallet::is_locked();
   }   

   bool wallet::is_locked()const
   {
      return my->_wallet_password == fc::sha512();
   }

   fc::time_point wallet::unlocked_until()const
   {
      return my->_scheduled_lock_time;
   }

   public_key_type  wallet::create_account( const std::string& account_name )
   { try {
      FC_ASSERT( is_open() );
      FC_ASSERT( is_unlocked() );

      auto current_account = my->_wallet_db.lookup_account( account_name );
      FC_ASSERT( !current_account.valid() );

      auto new_priv_key = my->_wallet_db.new_private_key( my->_wallet_password );
      auto new_pub_key  = new_priv_key.get_public_key();

      my->_wallet_db.add_account( account_name, new_pub_key );

      return new_pub_key;
   } FC_RETHROW_EXCEPTIONS( warn, "", ("account_name",account_name) ) }


   /**
    *  Creates a new account from an existing foreign private key
    */
   void wallet::import_account( const std::string& account_name, 
                                const std::string& wif_private_key )
   { try {
      auto current_account = my->_wallet_db.lookup_account( account_name );

      auto imported_public_key = import_wif_private_key( wif_private_key, std::string() );
      if( current_account.valid() )
      {
         FC_ASSERT( current_account->account_address == address( imported_public_key ) );
         import_wif_private_key( wif_private_key, account_name );
      }
      else
      {
         my->_wallet_db.add_account( account_name, imported_public_key );
         import_wif_private_key( wif_private_key, account_name );
      }
   } FC_RETHROW_EXCEPTIONS( warn, "", ("account_name",account_name) ) }


   /**
    *  Creates a new private key under the specified account. This key
    *  will not be valid for sending TITAN transactions to, but will
    *  be able to receive payments directly.
    */
   address  wallet::get_new_address( const std::string& account_name )
   { try {
      FC_ASSERT( is_open() );
      FC_ASSERT( is_unlocked() );

      auto current_account = my->_wallet_db.lookup_account( account_name );
      FC_ASSERT( !current_account.valid() );

      auto new_priv_key = my->_wallet_db.new_private_key( my->_wallet_password, 
                                                          current_account->account_address );
      return new_priv_key.get_public_key();
   } FC_RETHROW_EXCEPTIONS( warn, "", ("account_name",account_name) ) }


   /**
    *  A contact is an account for which this wallet does not have the private
    *  key.  
    *
    *  @param account_name - the name the account is known by to this wallet
    *  @param key - the public key that will be used for sending TITAN transactions
    *               to the account.
    */
   void  wallet::add_contact( const std::string& account_name, 
                              const public_key_type& key )
   { try {
      FC_ASSERT( is_open() );
      auto current_account = my->_wallet_db.lookup_account( account_name );
      if( current_account.valid() )
      {
         FC_ASSERT( current_account->account_address == address(key),
                    "Account with ${name} already exists", ("name",account_name) );
      }
      else
      {
         my->_wallet_db.add_account( account_name, key );
      }

   } FC_RETHROW_EXCEPTIONS( warn, "", ("account_name",account_name)("public_key",key) ) }


   void  wallet::rename_account( const std::string& old_account_name, 
                                 const std::string& new_account_name )
   { try {
      FC_ASSERT( is_open() );
      auto old_account = my->_wallet_db.lookup_account( old_account_name );
      FC_ASSERT( old_account.valid() );

      auto new_account = my->_wallet_db.lookup_account( new_account_name );
      FC_ASSERT( !new_account.valid() );

      my->_wallet_db.rename_account( old_account_name, new_account_name );
   } FC_RETHROW_EXCEPTIONS( warn, "", 
                ("old_account_name",old_account_name)
                ("new_account_name",new_account_name) ) }


   public_key_type  wallet::import_private_key( const private_key_type& key, 
                                                const std::string& account_name )
   { try {
      FC_ASSERT( is_open() );
      FC_ASSERT( is_unlocked() );

      auto current_account = my->_wallet_db.lookup_account( account_name );

      if( account_name != std::string() )
         FC_ASSERT( current_account.valid() );

      auto pub_key = key.get_public_key();
      address key_address(pub_key);
      auto current_key_record = my->_wallet_db.lookup_key( key_address );
      if( current_key_record.valid() )
      {
         FC_ASSERT( current_key_record->account_address == current_account->account_address );
         return current_key_record->public_key;
      }

      key_data new_key_data;
      if( current_account.valid() )
         new_key_data.account_address = current_account->account_address;
      new_key_data.encrypt_private_key( my->_wallet_password, key );

      my->_wallet_db.store_key( new_key_data );

      return pub_key;

   } FC_RETHROW_EXCEPTIONS( warn, "", ("account_name",account_name) ) }


   public_key_type wallet::import_wif_private_key( const std::string& wif_key, 
                                        const std::string& account_name )
   { try {
      FC_ASSERT( is_open() );
      FC_ASSERT( is_unlocked() );

      auto wif_bytes = fc::from_base58(wif_key);
      auto key_bytes = std::vector<char>(wif_bytes.begin() + 1, wif_bytes.end() - 4);
      auto key = fc::variant(key_bytes).as<fc::ecc::private_key>();
      auto check = fc::sha256::hash( wif_bytes.data(), wif_bytes.size() - 4 );

      if( 0 == memcmp( (char*)&check, wif_bytes.data() + wif_bytes.size() - 4, 4 ) )
         return import_private_key( key, account_name );
      
      FC_ASSERT( !"Error parsing WIF private key" );

   } FC_RETHROW_EXCEPTIONS( warn, "", ("account_name",account_name) ) }


   void  wallet::scan_chain( uint32_t start, uint32_t end  )
   { try {
      FC_ASSERT( is_open() );
      FC_ASSERT( is_unlocked() );

      auto min_end = std::min<size_t>( my->_blockchain->get_head_block_num(), end );
      FC_ASSERT( min_end >= start );

      auto account_priv_keys = my->_wallet_db.get_account_private_keys( my->_wallet_password );

      for( auto block_num = start; block_num <= min_end; ++block_num )
      {
         my->scan_block( block_num, account_priv_keys );
      }
   } FC_RETHROW_EXCEPTIONS( warn, "", ("start",start)("end",end) ) }


   void  wallet::sign_transaction( signed_transaction& trx, const std::unordered_set<address>& req_sigs )
   { try {
      for( auto addr : req_sigs )
      {
         trx.sign( get_private_key( addr ), my->_blockchain->chain_id()  );
      }
   } FC_RETHROW_EXCEPTIONS( warn, "", ("trx",trx)("req_sigs",req_sigs) ) }

   fc::ecc::private_key wallet::get_private_key( const address& addr )const
   { try {
      FC_ASSERT( is_open() );
      FC_ASSERT( is_unlocked() );

      auto key = my->_wallet_db.lookup_key( addr );
      FC_ASSERT( key.valid() );
      FC_ASSERT( key->has_private_key() );
      return key->decrypt_private_key( my->_wallet_password );
   } FC_RETHROW_EXCEPTIONS( warn, "", ("addr",addr) ) }


   std::vector<wallet_transaction_record>    wallet::get_transaction_history()const
   { try {
      FC_ASSERT( is_open() );

   } FC_RETHROW_EXCEPTIONS( warn, "" ) }


   /**
    *  If this wallet has any delegate keys, this method will return the time
    *  at which this wallet may produce a block.
    */
   fc::time_point_sec wallet::next_block_production_time()const
   { try {
      auto now = fc::time_point(bts::blockchain::now());
      uint32_t interval_number = now.sec_since_epoch() / BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
      uint32_t next_block_time = interval_number * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
      if( next_block_time == my->_blockchain->now().sec_since_epoch() )
         next_block_time += BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;

      uint32_t last_block_time = next_block_time + BTS_BLOCKCHAIN_NUM_DELEGATES * BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;

      while( next_block_time < last_block_time )
      {
         auto id = my->_blockchain->get_signing_delegate_id( fc::time_point_sec( next_block_time ) );
         auto delegate_record = my->_blockchain->get_name_record( id );
         auto key = my->_wallet_db.lookup_key( delegate_record->active_key );
         if( key.valid() && key->has_private_key() )
         {
            wlog( "next block time:  ${t}", ("t",fc::time_point_sec( next_block_time ) ) );
            return fc::time_point_sec( next_block_time ); 
         }
         next_block_time += BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC;
      }
      return fc::time_point_sec();
   } FC_RETHROW_EXCEPTIONS( warn, "" ) }

   void wallet::sign_block( signed_block_header& header )const
   { try {
      FC_ASSERT( is_unlocked() );
      auto signing_delegate_id = my->_blockchain->get_signing_delegate_id( header.timestamp );
      auto delegate_rec = my->_blockchain->get_name_record( signing_delegate_id );

      auto delegate_pub_key = my->_blockchain->get_signing_delegate_key( header.timestamp );
      auto delegate_key = get_private_key( address(delegate_pub_key) );
      FC_ASSERT( delegate_pub_key == delegate_key.get_public_key() );

      header.previous_secret = my->get_secret( 
                                      delegate_rec->delegate_info->last_block_num_produced,
                                      delegate_key );

      auto next_secret = my->get_secret( my->_blockchain->get_head_block_num() + 1, delegate_key );
      header.next_secret_hash = fc::ripemd160::hash( next_secret );

      header.sign(delegate_key);
      FC_ASSERT( header.validate_signee( delegate_pub_key ) );

   } FC_RETHROW_EXCEPTIONS( warn, "", ("header",header) ) }




} } // bts::wallet

