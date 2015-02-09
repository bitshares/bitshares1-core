#include <bts/wallet/config.hpp>
#include <bts/wallet/exceptions.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/wallet/wallet_impl.hpp>

#include <bts/blockchain/time.hpp>
#include <bts/cli/pretty.hpp>
#include <bts/utilities/git_revision.hpp>
#include <bts/utilities/key_conversion.hpp>

#include <algorithm>
#include <fstream>
#include <thread>

#include <bts/blockchain/fork_blocks.hpp>

namespace bts { namespace wallet {

namespace detail {

   wallet_impl::wallet_impl()
   {
       _num_scanner_threads = std::max( _num_scanner_threads, std::thread::hardware_concurrency() );

       _scanner_threads.reserve( _num_scanner_threads );
       for( uint32_t i = 0; i < _num_scanner_threads; ++i )
           _scanner_threads.push_back( std::unique_ptr<fc::thread>( new fc::thread( "wallet_scanner_" + std::to_string( i ) ) ) );
   }

   wallet_impl::~wallet_impl()
   {
   }

   void wallet_impl::state_changed( const pending_chain_state_ptr& state )
   {
       if( !self->is_open() || !self->is_unlocked() ) return;

       const auto last_unlocked_scanned_number = self->get_last_scanned_block_number();
       if ( _blockchain->get_head_block_num() < last_unlocked_scanned_number )
       {
           self->set_last_scanned_block_number( _blockchain->get_head_block_num() );
       }
   }

   void wallet_impl::block_applied( const block_summary& summary )
   {
       if( !self->is_open() || !self->is_unlocked() ) return;
       if( !self->get_transaction_scanning() ) return;
       if( summary.block_data.block_num <= self->get_last_scanned_block_number() ) return;

       self->start_scan( std::min( self->get_last_scanned_block_number() + 1, summary.block_data.block_num ), -1 );
   }

   vector<wallet_transaction_record> wallet_impl::get_pending_transactions()const
   {
       return _wallet_db.get_pending_transactions();
   }

   void wallet_impl::withdraw_to_transaction(
           const asset& amount_to_withdraw,
           const string& from_account_name,
           signed_transaction& trx,
           unordered_set<address>& required_signatures
           )
   { try {
      FC_ASSERT( !from_account_name.empty() );
      auto amount_remaining = amount_to_withdraw;

      const account_balance_record_summary_type balance_records = self->get_spendable_account_balance_records( from_account_name );
      if( balance_records.find( from_account_name ) == balance_records.end() )
         FC_CAPTURE_AND_THROW( insufficient_funds, (from_account_name)(amount_to_withdraw)(balance_records) );
      for( const auto& record : balance_records.at( from_account_name ) )
      {
          const asset balance = record.get_spendable_balance( _blockchain->get_pending_state()->now() );
          if( balance.amount <= 0 || balance.asset_id != amount_remaining.asset_id )
              continue;

          const auto owner = record.owner();
          if( !owner.valid() )
              continue;

          if( amount_remaining.amount > balance.amount )
          {
              trx.withdraw( record.id(), balance.amount );
              required_signatures.insert( *owner );
              amount_remaining -= balance;
          }
          else
          {
              trx.withdraw( record.id(), amount_remaining.amount );
              required_signatures.insert( *owner );
              return;
          }
      }

      const string required = _blockchain->to_pretty_asset( amount_to_withdraw );
      const string available = _blockchain->to_pretty_asset( amount_to_withdraw - amount_remaining );
      FC_CAPTURE_AND_THROW( insufficient_funds, (required)(available)(balance_records) );
   } FC_CAPTURE_AND_RETHROW( (amount_to_withdraw)(from_account_name)(trx)(required_signatures) ) }

   // TODO: What about retracted accounts?
   void wallet_impl::authorize_update(unordered_set<address>& required_signatures, oaccount_record account, bool need_owner_key )
   {
     owallet_key_record oauthority_key = _wallet_db.lookup_key(account->owner_key);

     // We do this check a lot and it doesn't fit conveniently into a loop because we're interested in two types of keys.
     // Instead, we extract it into a function.
     auto accept_key = [&]()->bool
     {
       if( oauthority_key.valid() && oauthority_key->has_private_key() )
       {
         required_signatures.insert( oauthority_key->get_address() );
         return true;
       }
       return false;
     };

     if( accept_key() ) return;

     if( !need_owner_key )
     {
       oauthority_key = _wallet_db.lookup_key(account->active_address());
       if( accept_key() ) return;
     }

     auto dot = account->name.find('.');
     while( dot != string::npos )
     {
       account = _blockchain->get_account_record( account->name.substr( dot+1 ) );
       FC_ASSERT( account.valid(), "Parent account is not valid; this should never happen." );
       oauthority_key = _wallet_db.lookup_key(account->active_address());
       if( accept_key() ) return;
       oauthority_key = _wallet_db.lookup_key(account->owner_key);
       if( accept_key() ) return;

       dot = account->name.find('.');
     }
   }

   secret_hash_type wallet_impl::get_secret( uint32_t block_num,
                                             const private_key_type& delegate_key )const
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

   void wallet_impl::reschedule_relocker()
   {
     if( !_relocker_done.valid() || _relocker_done.ready() )
       _relocker_done = fc::async( [this](){ relocker(); }, "wallet_relocker" );
   }

   void wallet_impl::relocker()
   {
       fc::time_point now = fc::time_point::now();
       ilog( "Starting wallet relocker task at time: ${t}", ("t", now) );
       if( !_scheduled_lock_time.valid() || now >= *_scheduled_lock_time )
       {
           /* Don't relock if we have enabled delegates */
           if( !self->get_my_delegates( enabled_delegate_status ).empty() )
           {
               ulog( "Wallet not automatically relocking because there are enabled delegates!" );
               return;
           }

           self->lock();
       }
       else
       {
         if (!_relocker_done.canceled())
         {
           ilog( "Scheduling wallet relocker task for time: ${t}", ("t", *_scheduled_lock_time) );
           _relocker_done = fc::schedule( [this](){ relocker(); },
                                          *_scheduled_lock_time,
                                          "wallet_relocker" );
         }
       }
   }

   void wallet_impl::start_scan_task( const uint32_t start_block_num, const uint32_t limit )
   { try {
       fc::oexception scan_exception;
       try
       {
           const uint32_t head_block_num = _blockchain->get_head_block_num();
           uint32_t current_block_num = std::max( start_block_num, 1u );
           uint32_t prev_block_num = current_block_num - 1;
           uint32_t count = 0;

           const bool track_progress = current_block_num < head_block_num && limit > 0;
           if( track_progress )
           {
               ulog( "Beginning scan at block ${n}...", ("n",current_block_num) );
               _scan_progress = 0;
           }

           const auto update_progress = [=]( const uint32_t count )
           {
               if( !track_progress ) return;
               const uint32_t total = std::min( { limit, head_block_num, head_block_num - current_block_num + 1 } );
               if( total == 0 ) return;
               _scan_progress = float( count ) / total;
               if( count % 10000 == 0 ) ulog( "Scanning ${p} done...", ("p",cli::pretty_percent( _scan_progress, 1 )) );
           };

           if( start_block_num == 0 )
           {
               scan_balances();
               scan_accounts();
           }
           else if( _dirty_accounts )
           {
               scan_accounts();
           }

           while( current_block_num <= head_block_num && count < limit && !_scan_in_progress.canceled() )
           {
               try
               {
                   scan_block( current_block_num );
               }
               catch( const fc::exception& e )
               {
                   elog( "Error scanning block ${n}: ${e}", ("n",current_block_num)("e",e.to_detail_string()) );
               }

               ++count;
               prev_block_num = current_block_num;
               ++current_block_num;

               if( count > 1 )
               {
                   update_progress( count );
                   if( count % 10 == 0 ) fc::usleep( fc::microseconds( 1 ) );
               }
           }

           self->set_last_scanned_block_number( std::max( prev_block_num, self->get_last_scanned_block_number() ) );

           if( track_progress )
           {
               _scan_progress = 1;
               ulog( "Scan complete." );
           }

           if( _dirty_balances ) scan_balances_experimental();
           if( _dirty_accounts ) scan_accounts();
       }
       catch( const fc::exception& e )
       {
           scan_exception = e;
       }

       if( scan_exception.valid() )
       {
           _scan_progress = -1;
           ulog( "Scan failure." );
           throw *scan_exception;
       }
   } FC_CAPTURE_AND_RETHROW( (start_block_num)(limit) ) }

   void wallet_impl::upgrade_version()
   {
       const uint32_t current_version = self->get_version();
       if( current_version > BTS_WALLET_VERSION )
       {
           FC_THROW_EXCEPTION( unsupported_version, "Wallet version newer than client supports!",
                               ("wallet_version",current_version)("supported_version",BTS_WALLET_VERSION) );
       }
       else if( current_version == BTS_WALLET_VERSION )
       {
           return;
       }

       ulog( "Upgrading wallet..." );
       std::exception_ptr upgrade_failure_exception;
       try
       {
           self->auto_backup( "version_upgrade" );

           if( current_version < 100 )
           {
               self->set_automatic_backups( true );
               self->set_transaction_scanning( self->get_my_delegates( enabled_delegate_status ).empty() );

               /* Check for old index format genesis claim virtual transactions */
               auto present = false;
               _blockchain->scan_balances( [&]( const balance_record& bal_rec )
               {
                    if( !bal_rec.snapshot_info.valid() ) return;
                    const auto id = bal_rec.id().addr;
                    present |= _wallet_db.lookup_transaction( id ).valid();
               } );

               if( present )
               {
                   const function<void( void )> rescan = [&]()
                   {
                       /* Upgrade genesis claim virtual transaction indexes */
                       _blockchain->scan_balances( [&]( const balance_record& bal_rec )
                       {
                            if( !bal_rec.snapshot_info.valid() ) return;
                            const auto id = bal_rec.id().addr;
                            _wallet_db.remove_transaction( id );
                       } );
                       scan_balances();
                   };
                   _unlocked_upgrade_tasks.push_back( rescan );
               }
           }

           if( current_version < 101 )
           {
               /* Check for old index format market order virtual transactions */
               auto present = false;
               const auto items = _wallet_db.get_transactions();
               for( const auto& item : items )
               {
                   const auto id = item.first;
                   const auto trx_rec = item.second;
                   if( trx_rec.is_virtual && trx_rec.is_market )
                   {
                       present = true;
                       _wallet_db.remove_transaction( id );
                   }
               }
               if( present )
               {
                   const auto start = 1;
                   const auto end = _blockchain->get_head_block_num();

                   /* Upgrade market order virtual transaction indexes */
                   for( auto block_num = start; block_num <= end; block_num++ )
                   {
                       const auto block_timestamp = _blockchain->get_block_header( block_num ).timestamp;
                       const auto market_trxs = _blockchain->get_market_transactions( block_num );
                       for( const auto& market_trx : market_trxs )
                           scan_market_transaction( market_trx, block_num, block_timestamp );
                   }
               }
           }

           if( current_version < 102 )
           {
               self->set_transaction_fee( asset( BTS_WALLET_DEFAULT_TRANSACTION_FEE ) );
           }

           if( current_version < 106 )
           {
               self->set_transaction_expiration( BTS_WALLET_DEFAULT_TRANSACTION_EXPIRATION_SEC );
           }

           if( current_version < 107 )
           {
               const auto items = _wallet_db.get_transactions();
               for( const auto& item : items )
               {
                   const auto id = item.first;
                   const auto trx_rec = item.second;
                   if( trx_rec.is_virtual && trx_rec.is_market && trx_rec.block_num == 554801 )
                       _wallet_db.remove_transaction( id );
               }
           }

           if( current_version < 109 )
           {
               const function<void( void )> repair = [&]()
               {
                   _wallet_db.repair_records( _wallet_password );
               };
               _unlocked_upgrade_tasks.push_back( repair );
           }

           if( _unlocked_upgrade_tasks.empty() )
           {
               self->set_version( BTS_WALLET_VERSION );
               ulog( "Wallet successfully upgraded." );
           }
           else
           {
               ulog( "Please unlock your wallet to complete the upgrade..." );
           }
       }
       catch( ... )
       {
           upgrade_failure_exception = std::current_exception();
       }

       if (upgrade_failure_exception)
       {
           ulog( "Wallet upgrade failure." );
           std::rethrow_exception(upgrade_failure_exception);
       }
   }

   void wallet_impl::upgrade_version_unlocked()
   {
       if( _unlocked_upgrade_tasks.empty() ) return;

       ulog( "Continuing wallet upgrade..." );
       std::exception_ptr upgrade_failure_exception;
       try
       {
           for( const auto& task : _unlocked_upgrade_tasks ) task();
           _unlocked_upgrade_tasks.clear();
           self->set_version( BTS_WALLET_VERSION );
           ulog( "Wallet successfully upgraded." );
       }
       catch( ... )
       {
           upgrade_failure_exception = std::current_exception();
       }

       if (upgrade_failure_exception)
       {
           ulog( "Wallet upgrade failure." );
           std::rethrow_exception(upgrade_failure_exception);
       }
   }

   void wallet_impl::apply_sell_order_to_builder(
                                            transaction_builder_ptr builder,
                                            const string& from_account_name,
                                            const string& sell_quantity,
                                            const string& sell_quantity_symbol,
                                            const string& price_limit,
                                            const string& price_symbol,
                                            const string& relative_price_str,
                                            bool allow_stupid
                                            )
   {
       if( !is_receive_account(from_account_name) )
           FC_CAPTURE_AND_THROW( unknown_receive_account, (from_account_name) );

	   const wallet_account_record from_account =
	       self->get_account( from_account_name );

	   // TODO:  allow_stupid
	   oasset_record sell_arec = _blockchain->get_asset_record( sell_quantity_symbol );
	   oasset_record price_arec = _blockchain->get_asset_record( price_symbol );

       // TODO: Add symbol to error messages, use exception ID instead of assert
	   FC_ASSERT( sell_arec.valid(), "could not find record for asset being sold" );
	   FC_ASSERT( price_arec.valid(), "could not find record for asset being bought" );

	   const asset_id_type sell_id = sell_arec->id;
	   const asset_id_type price_id = price_arec->id;

	   const asset ugly_sell_qty = _blockchain->to_ugly_asset( sell_quantity, sell_quantity_symbol );

       if( sell_id == price_id )
          FC_CAPTURE_AND_THROW( invalid_market, (sell_quantity_symbol)(price_symbol) );

       const bool is_bid = (sell_id > price_id);
       if( is_bid )
       {
		   // n.b. bid base/quote is opposite of ask
		   const asset_id_type base_id = price_arec->id;
		   const asset_id_type quote_id = sell_arec->id;
		   const string& base_symbol = price_symbol;
		   const string& quote_symbol = sell_quantity_symbol;

   	       price relative_price = str_to_relative_price(
   	           relative_price_str, base_symbol, quote_symbol );

           oprice limit_price;

           if( (price_limit != "") &&
               (price_limit != "0")
             )
           {
                limit_price = _blockchain->to_ugly_asset( price_limit, price_symbol )
                             / _blockchain->to_ugly_asset( "1", sell_quantity_symbol );
                if((limit_price->ratio == fc::uint128_t(0)) || (limit_price->is_infinite()))
                    limit_price.reset();
           }

		   if( relative_price.ratio > 0 )
		   {
			   // relative bid.
               relative_price.base_asset_id = base_id;
               relative_price.quote_asset_id = quote_id;

               builder->submit_relative_bid(
                   from_account,
                   ugly_sell_qty,
                   relative_price,
                   limit_price);

		   }
		   else
		   {
			   FC_ASSERT( limit_price.valid(), "Must specify at least one of rel. price or limit price" );

			   // absolute bid.
               builder->submit_bid(
                   from_account,
                   ugly_sell_qty,
                   *limit_price);
		   }
	   }
	   else
	   {
		   // n.b. ask base/quote is opposite of bid
		   const asset_id_type base_id = sell_arec->id;
		   const asset_id_type quote_id = price_arec->id;
		   const string& base_symbol = sell_quantity_symbol;
		   const string& quote_symbol = price_symbol;

   	       price relative_price = str_to_relative_price(
   	           relative_price_str, base_symbol, quote_symbol );

           oprice limit_price;

           if( (price_limit != "") &&
               (price_limit != "0")
             )
           {
               limit_price = _blockchain->to_ugly_asset( price_limit, price_symbol )
                           / _blockchain->to_ugly_asset( "1", sell_quantity_symbol );
               if((limit_price->ratio == fc::uint128_t(0)) || (limit_price->is_infinite()))
                    limit_price.reset();
           }

		   if( relative_price.ratio > 0 )
		   {
			   // relative ask.
               relative_price.base_asset_id = base_id;
               relative_price.quote_asset_id = quote_id;

               builder->submit_relative_ask(
                   from_account,
                   ugly_sell_qty,
                   relative_price,
                   limit_price);
		   }
		   else
		   {
			   FC_ASSERT( limit_price.valid(), "Must specify at least one of rel. price or limit price" );

			   // absolute ask.
               builder->submit_ask(
                   from_account,
                   ugly_sell_qty,
                   *limit_price);
		   }
	   }
	   return;
   }

   void wallet_impl::apply_order_to_builder(order_type_enum order_type,
                                            transaction_builder_ptr builder,
                                            const string& account_name,
                                            const string& balance,
                                            const string& order_price,
                                            const string& base_symbol,
                                            const string& quote_symbol,
                                            bool needs_satoshi_conversion,
                                            const string& limit
                                            )
   {
      FC_ASSERT( (order_type != relative_ask_order)
              && (order_type != relative_bid_order),
              "relative orders not supported by apply_order_to_builder"
              );

	  // description of "balance" parameter:
	  // in the case of ask, relative ask, relative bid, "balance" is an existing balance to be sold
	  // in the case of bid, "balance" is desired quantity to buy
	  // TODO:  What is balance in case of short and cover?

      // is_ba : is bid or ask
      // is_rel_ba : is relative bid or relative ask
      const bool is_ba       = ((order_type ==          bid_order) | (order_type ==          ask_order));
      const bool is_rel_ba   = ((order_type == relative_bid_order) | (order_type == relative_ask_order));
      const bool is_short    =  (order_type ==        short_order);

	  if( !(is_ba | is_rel_ba | is_short) )
         FC_THROW_EXCEPTION( invalid_operation, "This function only supports bids, asks and shorts." );

      if( !is_receive_account(account_name) )
         FC_CAPTURE_AND_THROW( unknown_receive_account, (account_name) );
      asset quantity = _blockchain->to_ugly_asset(balance, base_symbol);

      // For reference, these are the available order types:
      //
      // bid_order,
      // ask_order,
      // short_order,
      // cover_order,
      // relative_bid_order,
      // relative_ask_order

      const bool is_zero_price_allowed = is_short;

      if( quantity.amount < 0 )
         FC_CAPTURE_AND_THROW( invalid_asset_amount, (balance) );
      if( quantity.amount == 0 )
         FC_CAPTURE_AND_THROW( invalid_asset_amount, (balance) );
      if( atof(order_price.c_str()) < 0 )
        FC_CAPTURE_AND_THROW( invalid_price, (order_price) );
      if( (!is_zero_price_allowed) && atof(order_price.c_str()) == 0 )
        FC_CAPTURE_AND_THROW( invalid_price, (order_price) );

      // Satoshi conversion (aka precision dance) is because the price
      //    passed into this function is a ratio of currency *units*
      //    but the price passed on to the builder is a ratio
      //    of *satoshis*.  If the base and quote assets have different
      //    precision, then a numerical conversion will be needed.
      //
      // For example, 1 USD = 10000 USD-satoshi, 1 BTS = 100000 BTS-satoshi
      //
      // Thus, 0.02 USD / BTS expressed as a ratio of satoshis would be
      //       0.02 USD / BTS * (1 BTS / 100000 BTS-satoshi) * (10000 USD-satoshi / 1 USD) = 0.002 USD-satoshi per BTS-satoshi.
      //
      // HOWEVER in the case of relative orders, the price is a percentage
      //    which should not get converted.
      //
      // And in the case of a short, the order_price is an APR which
      //    does not get converted, but the limit price is a price,
      //    which does.

      price price_arg = _blockchain->to_ugly_price(order_price,
                                                   base_symbol,
                                                   quote_symbol,
                                                   needs_satoshi_conversion);

      // price_limit affects shorts and relative orders.
      oprice price_limit;
      if( !limit.empty() && atof(limit.c_str()) > 0 )
         price_limit = _blockchain->to_ugly_price(limit, base_symbol, quote_symbol, true);

      if( order_type == bid_order )
         builder->submit_bid(self->get_account(account_name), quantity, price_arg);
      else if( order_type == ask_order )
         builder->submit_ask(self->get_account(account_name), quantity, price_arg);
      else if( order_type == short_order )
      {
		 static fc::uint128 max_apr = fc::uint128( 1000, 0 );
         FC_ASSERT( price_arg.ratio < max_apr, "APR must be less than 1000%" );
         price_arg.ratio /= 100;
         builder->submit_short(self->get_account(account_name), quantity, price_arg, price_limit);
      }
   }

   void wallet_impl::create_file( const path& wallet_file_path,
                                  const string& password,
                                  const string& brainkey )
   { try {
      FC_ASSERT( self->is_enabled(), "Wallet is disabled in this client!" );

      if( fc::exists( wallet_file_path ) )
          FC_THROW_EXCEPTION( wallet_already_exists, "Wallet file already exists!", ("wallet_file_path",wallet_file_path) );

      if( password.size() < BTS_WALLET_MIN_PASSWORD_LENGTH )
          FC_THROW_EXCEPTION( password_too_short, "Password too short!", ("size",password.size()) );

      std::exception_ptr create_file_failure;
      try
      {
          self->close();

          _wallet_db.open( wallet_file_path );
          _wallet_password = fc::sha512::hash( password.c_str(), password.size() );

          master_key new_master_key;
          extended_private_key epk;
          if( !brainkey.empty() )
          {
             auto base = fc::sha512::hash( brainkey.c_str(), brainkey.size() );

             /* strengthen the key a bit */
             for( uint32_t i = 0; i < 100ll*1000ll; ++i )
                base = fc::sha512::hash( base );

             epk = extended_private_key( base );
          }
          else
          {
             wlog( "generating random" );
             epk = extended_private_key( private_key_type::generate() );
          }

          _wallet_db.set_master_key( epk, _wallet_password);

          self->set_version( BTS_WALLET_VERSION );
          self->set_automatic_backups( true );
          self->set_transaction_scanning( true );
          self->set_last_scanned_block_number( _blockchain->get_head_block_num() );
          self->set_transaction_fee( asset( BTS_WALLET_DEFAULT_TRANSACTION_FEE ) );
          self->set_transaction_expiration( BTS_WALLET_DEFAULT_TRANSACTION_EXPIRATION_SEC );

          _wallet_db.close();
          _wallet_db.open( wallet_file_path );
          _current_wallet_path = wallet_file_path;

          FC_ASSERT( _wallet_db.validate_password( _wallet_password ) );
      }
      catch( ... )
      {
          create_file_failure = std::current_exception();
      }

      if (create_file_failure)
      {
          self->close();
          fc::remove_all( wallet_file_path );
          std::rethrow_exception(create_file_failure);
      }
   } FC_RETHROW_EXCEPTIONS( warn, "Unable to create wallet '${wallet_file_path}'", ("wallet_file_path",wallet_file_path) ) }

   void wallet_impl::open_file( const path& wallet_file_path )
   { try {
      FC_ASSERT( self->is_enabled(), "Wallet is disabled in this client!" );

      if ( !fc::exists( wallet_file_path ) )
         FC_THROW_EXCEPTION( no_such_wallet, "No such wallet exists!", ("wallet_file_path", wallet_file_path) );

      if( self->is_open() && _current_wallet_path == wallet_file_path )
          return;

      std::exception_ptr open_file_failure;
      try
      {
          self->close();
          _current_wallet_path = wallet_file_path;
          _wallet_db.open( wallet_file_path );
          upgrade_version();
          self->set_data_directory( fc::absolute( wallet_file_path.parent_path() ) );
      }
      catch( ... )
      {
          open_file_failure = std::current_exception();
      }

      if (open_file_failure)
      {
          self->close();
          std::rethrow_exception(open_file_failure);
      }
   } FC_RETHROW_EXCEPTIONS( warn, "Unable to open wallet ${wallet_file_path}", ("wallet_file_path",wallet_file_path) ) }

   /**
    *  Creates a new private key under the specified account. This key
    *  will not be valid for sending TITAN transactions to, but will
    *  be able to receive payments directly.
    */
   private_key_type wallet_impl::get_new_private_key( const string& account_name )
   { try {
      if( NOT self->is_open() ) FC_CAPTURE_AND_THROW( wallet_closed );
      if( NOT self->is_unlocked() ) FC_CAPTURE_AND_THROW( wallet_locked );
      if( NOT is_receive_account(account_name) )
          FC_CAPTURE_AND_THROW( unknown_receive_account, (account_name) );

      const auto current_account = _wallet_db.lookup_account( account_name );
      FC_ASSERT( current_account.valid() );

      return _wallet_db.generate_new_account_child_key( _wallet_password, account_name );
   } FC_CAPTURE_AND_RETHROW( (account_name) ) }

   public_key_type wallet_impl::get_new_public_key( const string& account_name )
   { try {
      return get_new_private_key( account_name ).get_public_key();
   } FC_CAPTURE_AND_RETHROW( (account_name) ) }

   address wallet_impl::get_new_address( const string& account_name, const string& label )
   { try {
       auto addr = address( get_new_public_key( account_name ) );
       auto okey = _wallet_db.lookup_key( addr );
       FC_ASSERT( okey.valid(), "Key I just created does not exist" );

       _wallet_db.store_key( *okey );
       return addr;
   } FC_CAPTURE_AND_RETHROW( (account_name) ) }

   slate_id_type wallet_impl::set_delegate_slate( signed_transaction& transaction, const vote_strategy strategy )const
   { try {
       slate_id_type slate_id = 0;
       if( strategy == vote_none ) return slate_id;

       const slate_record record = get_delegate_slate( strategy );
       slate_id = record.id();
       if( slate_id == 0 ) return slate_id;

       if( !_blockchain->get_slate_record( slate_id ).valid() ) transaction.define_slate( record.slate );
       transaction.set_slates( slate_id );
       return slate_id;
   } FC_CAPTURE_AND_RETHROW( (transaction)(strategy) ) }

   /**
    *  Any account for which this wallet owns the active private key.
    */
   bool wallet_impl::is_receive_account( const string& account_name )const
   {
      FC_ASSERT( self->is_open() );
      if( !_blockchain->is_valid_account_name( account_name ) ) return false;
      auto opt_account = _wallet_db.lookup_account( account_name );
      if( !opt_account.valid() ) return false;
      auto opt_key = _wallet_db.lookup_key( opt_account->active_address() );
      if( !opt_key.valid() ) return false;
      return opt_key->has_private_key();
   }

   bool wallet_impl::is_unique_account( const string& account_name )const
   {
      //There are two possibilities here. First, the wallet has multiple records named account_name
      //Second, the wallet has a different record named account_name than the blockchain does.

      //Check that the wallet has at most one account named account_name
      auto known_accounts = _wallet_db.get_accounts();
      bool found = false;
      for( const auto& known_account : known_accounts )
      {
        if( known_account.second.name == account_name )
        {
          if( found ) return false;
          found = true;
        }
      }

      if( !found )
        //The wallet does not contain an account with this name. No conflict is possible.
        return true;

      //The wallet has an account named account_name. Check that it matches with the blockchain
      auto local_account      = _wallet_db.lookup_account( account_name );
      auto registered_account = _blockchain->get_account_record( account_name );
      if( local_account && registered_account )
         return local_account->owner_key == registered_account->owner_key;
      return local_account || registered_account;
   }

   /**
    *  Select a slate of delegates from those approved by this wallet. Specify
    *  strategy as vote_none, vote_all, or vote_random. The slate
    *  returned will contain no more than BTS_BLOCKCHAIN_MAX_SLATE_SIZE delegates.
    */
   slate_record wallet_impl::get_delegate_slate( const vote_strategy strategy )const
   {
      if( strategy == vote_none )
         return slate_record();

      vector<account_id_type> for_candidates;

      const auto account_items = _wallet_db.get_accounts();
      for( const auto& item : account_items )
      {
          const auto account_record = item.second;
          if( !account_record.is_delegate() && strategy != vote_recommended ) continue;
          if( account_record.approved <= 0 ) continue;
          for_candidates.push_back( account_record.id );
      }
      std::random_shuffle( for_candidates.begin(), for_candidates.end() );

      size_t slate_size = 0;
      if( strategy == vote_all )
      {
          slate_size = std::min<size_t>( BTS_BLOCKCHAIN_MAX_SLATE_SIZE, for_candidates.size() );
      }
      else if( strategy == vote_random )
      {
          slate_size = std::min<size_t>( BTS_BLOCKCHAIN_MAX_SLATE_SIZE / 3, for_candidates.size() );
          slate_size = rand() % ( slate_size + 1 );
      }
      else if( strategy == vote_recommended && for_candidates.size() < BTS_BLOCKCHAIN_MAX_SLATE_SIZE )
      {
          unordered_map<account_id_type, int> recommended_candidate_ranks;

          //Tally up the recommendation count for all delegates recommended by delegates I approve of
          for( const auto approved_candidate : for_candidates )
          {
            oaccount_record candidate_record = _blockchain->get_account_record(approved_candidate);
            if( !candidate_record.valid() ) continue;
            if( candidate_record->is_retracted() ) continue;

            if( !candidate_record->public_data.is_object()
                || !candidate_record->public_data.get_object().contains("slate_id"))
              continue;

            if( !candidate_record->public_data.get_object()["slate_id"].is_uint64() )
            {
              //Delegate is doing something non-kosher with their slate_id. Disapprove of them.
              self->set_account_approval( candidate_record->name, -1 );
              continue;
            }

            oslate_record recommendations = _blockchain->get_slate_record(candidate_record->public_data.get_object()["slate_id"].as<slate_id_type>());
            if( !recommendations.valid() )
            {
              //Delegate is doing something non-kosher with their slate_id. Disapprove of them.
              self->set_account_approval( candidate_record->name, -1 );
              continue;
            }

            for( const auto recommended_candidate : recommendations->slate )
              ++recommended_candidate_ranks[recommended_candidate];
          }

          //Disqualify non-delegates and delegates I actively disapprove of
          for( const auto& acct_rec : account_items )
             if( !acct_rec.second.is_delegate() || acct_rec.second.approved < 0 )
                recommended_candidate_ranks.erase(acct_rec.second.id);

          //Remove from rankings candidates I already approve of
          for( const auto approved_id : for_candidates )
            if( recommended_candidate_ranks.find(approved_id) != recommended_candidate_ranks.end() )
              recommended_candidate_ranks.erase(approved_id);

          //Remove non-delegates from for_candidates
          vector<account_id_type> delegates;
          for( const auto id : for_candidates )
          {
              const oaccount_record record = _blockchain->get_account_record( id );
              if( !record.valid() ) continue;
              if( !record->is_delegate() ) continue;
              if( record->is_retracted() ) continue;
              delegates.push_back(id);
          }
          for_candidates = delegates;

          //While I can vote for more candidates, and there are more recommendations to vote for...
          while( for_candidates.size() < BTS_BLOCKCHAIN_MAX_SLATE_SIZE && recommended_candidate_ranks.size() > 0 )
          {
            int best_rank = 0;
            account_id_type best_ranked_candidate;

            //Add highest-ranked candidate to my list to vote for and remove him from rankings
            for( const auto& ranked_candidate : recommended_candidate_ranks )
              if( ranked_candidate.second > best_rank )
              {
                best_rank = ranked_candidate.second;
                best_ranked_candidate = ranked_candidate.first;
              }

            for_candidates.push_back(best_ranked_candidate);
            recommended_candidate_ranks.erase(best_ranked_candidate);
          }

          slate_size = for_candidates.size();
      }

      slate_record record;
      for( const account_id_type id : for_candidates ) record.slate.insert( id );
      FC_ASSERT( record.slate.size() <= BTS_BLOCKCHAIN_MAX_SLATE_SIZE );

      return record;
   }

   price wallet_impl::str_to_relative_price( const string& str, const string& base_symbol, const string& quote_symbol )
   {
	   size_t n = str.length();

	   string s;
	   bool divide_by_100 = false;

       FC_ASSERT( n > 0 );

	   if( str[n-1] == '%' )
	   {
		   s = str.substr( 0, n-1 );
		   divide_by_100 = true;
	   }
	   else
	       s = str;

	   price result = _blockchain->to_ugly_price( s, base_symbol, quote_symbol, false );

	   if( divide_by_100 )
		   result.ratio /= 100;
	   return result;
   }

} // bts::wallet::detail

   wallet::wallet( chain_database_ptr blockchain, bool enabled )
   : my( new detail::wallet_impl() )
   {
      my->self = this;
      my->_is_enabled = enabled;
      my->_blockchain = blockchain;
      my->_blockchain->add_observer( my.get() );
   }

   wallet::~wallet()
   {
      close();
   }

   void wallet::set_data_directory( const path& data_dir )
   {
      my->_data_directory = data_dir;
   }

   path wallet::get_data_directory()const
   {
      return my->_data_directory;
   }

   void wallet::create( const string& wallet_name,
                        const string& password,
                        const string& brainkey )
   { try {
      FC_ASSERT(is_enabled(), "Wallet is disabled in this client!");

      if( !my->_blockchain->is_valid_account_name( wallet_name ) )
          FC_THROW_EXCEPTION( invalid_name, "Invalid name for a wallet!", ("wallet_name",wallet_name) );

      auto wallet_file_path = fc::absolute( get_data_directory() ) / wallet_name;
      if( fc::exists( wallet_file_path ) )
          FC_THROW_EXCEPTION( wallet_already_exists, "Wallet name already exists!", ("wallet_name",wallet_name) );

      if( password.size() < BTS_WALLET_MIN_PASSWORD_LENGTH )
          FC_THROW_EXCEPTION( password_too_short, "Password too short!", ("size",password.size()) );

      std::exception_ptr wallet_create_failure;
      try
      {
          my->create_file( wallet_file_path, password, brainkey );
          open( wallet_name );
          unlock( password, BTS_WALLET_DEFAULT_UNLOCK_TIME_SEC );
      }
      catch( ... )
      {
          wallet_create_failure = std::current_exception();
      }

      if (wallet_create_failure)
      {
          close();
          std::rethrow_exception(wallet_create_failure);
      }
   } FC_RETHROW_EXCEPTIONS( warn, "Unable to create wallet '${wallet_name}'", ("wallet_name",wallet_name) ) }

   void wallet::open( const string& wallet_name )
   { try {
      FC_ASSERT(is_enabled(), "Wallet is disabled in this client!");

      if( !my->_blockchain->is_valid_account_name( wallet_name ) )
          FC_THROW_EXCEPTION( invalid_name, "Invalid name for a wallet!", ("wallet_name",wallet_name) );

      auto wallet_file_path = fc::absolute( get_data_directory() ) / wallet_name;
      if ( !fc::exists( wallet_file_path ) )
         FC_THROW_EXCEPTION( no_such_wallet, "No such wallet exists!", ("wallet_name", wallet_name) );

      std::exception_ptr open_file_failure;
      try
      {
          my->open_file( wallet_file_path );
      }
      catch( ... )
      {
          open_file_failure = std::current_exception();
      }

      if (open_file_failure)
      {
          close();
          std::rethrow_exception(open_file_failure);
      }

      my->scan_accounts();
      my->scan_balances_experimental();
   } FC_CAPTURE_AND_RETHROW( (wallet_name) ) }

   void wallet::close()
   { try {
      lock();

      try
      {
        ilog( "Canceling wallet relocker task..." );
        my->_relocker_done.cancel_and_wait("wallet::close()");
        ilog( "Wallet relocker task canceled" );
      }
      catch( const fc::exception& e )
      {
        wlog("Unexpected exception from wallet's relocker() : ${e}", ("e", e));
      }
      catch( ... )
      {
        wlog("Unexpected exception from wallet's relocker()");
      }

      my->_balance_records.clear();
      my->_dirty_balances = true;
      my->_wallet_db.close();
      my->_current_wallet_path = fc::path();
   } FC_CAPTURE_AND_RETHROW() }

   bool wallet::is_enabled() const
   {
      return my->_is_enabled;
   }

   bool wallet::is_open()const
   {
      return my->_wallet_db.is_open();
   }

   string wallet::get_wallet_name()const
   {
      return my->_current_wallet_path.filename().generic_string();
   }

   void wallet::export_to_json( const path& filename )const
   { try {
      if( fc::exists( filename ) )
          FC_THROW_EXCEPTION( file_already_exists, "Filename to export to already exists!", ("filename",filename) );

      FC_ASSERT( is_open() );
      my->_wallet_db.export_to_json( filename );
   } FC_CAPTURE_AND_RETHROW( (filename) ) }

   void wallet::create_from_json( const path& filename, const string& wallet_name, const string& passphrase )
   { try {
      FC_ASSERT(is_enabled(), "Wallet is disabled in this client!");

      if( !fc::exists( filename ) )
          FC_THROW_EXCEPTION( file_not_found, "Filename to import from could not be found!", ("filename",filename) );

      if( !my->_blockchain->is_valid_account_name( wallet_name ) )
          FC_THROW_EXCEPTION( invalid_wallet_name, "Invalid name for a wallet!", ("wallet_name",wallet_name) );

      create( wallet_name, passphrase );
      std::exception_ptr import_failure;
      try
      {
          set_version( 0 );
          my->_wallet_db.import_from_json( filename );
          close();
          open( wallet_name );
          unlock( passphrase, BTS_WALLET_DEFAULT_UNLOCK_TIME_SEC );
      }
      catch( ... )
      {
          import_failure = std::current_exception();
      }

      if (import_failure)
      {
          close();
          fc::path wallet_file_path = fc::absolute( get_data_directory() ) / wallet_name;
          fc::remove_all( wallet_file_path );
          std::rethrow_exception(import_failure);
      }
   } FC_CAPTURE_AND_RETHROW( (filename)(wallet_name) ) }

   void wallet::auto_backup( const string& reason )const
   { try {
      if( !get_automatic_backups() ) return;
      ulog( "Backing up wallet..." );
      fc::path wallet_path = my->_current_wallet_path;
      std::string wallet_name = wallet_path.filename().string();
      fc::path wallet_dir = wallet_path.parent_path();
      fc::path backup_path;
      while( true )
      {
          fc::time_point_sec now( time_point::now() );
          std::string backup_filename = wallet_name + "-" + now.to_non_delimited_iso_string();
          if( !reason.empty() ) backup_filename += "-" + reason;
          backup_filename += ".json";
          backup_path = wallet_dir / ".backups" / wallet_name / backup_filename;
          if( !fc::exists( backup_path ) ) break;
          fc::usleep( fc::seconds( 1 ) );
      }
      export_to_json( backup_path );
      ulog( "Wallet automatically backed up to: ${f}", ("f",backup_path) );
   } FC_CAPTURE_AND_RETHROW() }

   void wallet::set_version( uint32_t v )
   { try {
       FC_ASSERT( is_open() );
       my->_wallet_db.set_property( version, variant( v ) );
   } FC_CAPTURE_AND_RETHROW() }

   uint32_t wallet::get_version()const
   { try {
       FC_ASSERT( is_open() );
       try
       {
           return my->_wallet_db.get_property( version ).as<uint32_t>();
       }
       catch( ... )
       {
       }
       return 0;
   } FC_CAPTURE_AND_RETHROW() }

   void wallet::set_automatic_backups( bool enabled )
   { try {
       FC_ASSERT( is_open() );
       my->_wallet_db.set_property( automatic_backups, variant( enabled ) );
   } FC_CAPTURE_AND_RETHROW() }

   bool wallet::get_automatic_backups()const
   { try {
       FC_ASSERT( is_open() );
       try
       {
           return my->_wallet_db.get_property( automatic_backups ).as<bool>();
       }
       catch( ... )
       {
       }
       return true;
   } FC_CAPTURE_AND_RETHROW() }

   void wallet::set_transaction_scanning( bool enabled )
   { try {
       FC_ASSERT( is_open() );
       my->_wallet_db.set_property( transaction_scanning, variant( enabled ) );
   } FC_CAPTURE_AND_RETHROW() }

   bool wallet::get_transaction_scanning()const
   { try {
       FC_ASSERT( is_open() );

       if( list_my_accounts().empty() )
           return false;

       try
       {
           return my->_wallet_db.get_property( transaction_scanning ).as<bool>();
       }
       catch( ... )
       {
       }
       return true;
   } FC_CAPTURE_AND_RETHROW() }

   void wallet::unlock( const string& password, uint32_t timeout_seconds )
   { try {
      std::exception_ptr unlock_error;
      try
      {
          FC_ASSERT( is_open() );

          if( timeout_seconds < 1 )
              FC_THROW_EXCEPTION( invalid_timeout, "Invalid timeout!" );

          fc::time_point now = fc::time_point::now();
          fc::time_point new_lock_time = now + fc::seconds( timeout_seconds );
          if( new_lock_time.sec_since_epoch() <= now.sec_since_epoch() )
              FC_THROW_EXCEPTION( invalid_timeout, "Invalid timeout!" );

          if( password.size() < BTS_WALLET_MIN_PASSWORD_LENGTH )
              FC_THROW_EXCEPTION( password_too_short, "Invalid password!" );

          my->_wallet_password = fc::sha512::hash( password.c_str(), password.size() );
          if( !my->_wallet_db.validate_password( my->_wallet_password ) )
              FC_THROW_EXCEPTION( invalid_password, "Invalid password!" );

          my->upgrade_version_unlocked();

          my->_scheduled_lock_time = new_lock_time;
          ilog( "Wallet unlocked at time: ${t}", ("t", fc::time_point_sec(now)) );
          my->reschedule_relocker();
          wallet_lock_state_changed( false );
          ilog( "Wallet unlocked until time: ${t}", ("t", fc::time_point_sec(*my->_scheduled_lock_time)) );

          my->scan_accounts();
      }
      catch( ... )
      {
          unlock_error = std::current_exception();
      }

      if (unlock_error)
      {
          lock();
          std::rethrow_exception(unlock_error);
      }
   } FC_CAPTURE_AND_RETHROW( (timeout_seconds) ) }

   void wallet::lock()
   {
      cancel_scan();

      try
      {
        my->_login_map_cleaner_done.cancel_and_wait("wallet::lock()");
      }
      catch( const fc::exception& e )
      {
        wlog("Unexpected exception from wallet's login_map_cleaner() : ${e}", ("e", e));
      }
      catch( ... )
      {
        wlog("Unexpected exception from wallet's login_map_cleaner()");
      }

      my->_stealth_private_keys.clear();
      my->_dirty_accounts = true;
      my->_wallet_password = fc::sha512();
      my->_scheduled_lock_time = fc::optional<fc::time_point>();

      wallet_lock_state_changed( true );
      ilog( "Wallet locked at time: ${t}", ("t",blockchain::now()) );
   }

   void wallet::change_passphrase( const string& new_passphrase )
   { try {
      if( NOT is_open() ) FC_CAPTURE_AND_THROW( wallet_closed );
      if( NOT is_unlocked() ) FC_CAPTURE_AND_THROW( wallet_locked );
      if( new_passphrase.size() < BTS_WALLET_MIN_PASSWORD_LENGTH ) FC_CAPTURE_AND_THROW( password_too_short );

      auto new_password = fc::sha512::hash( new_passphrase.c_str(), new_passphrase.size() );

      my->_wallet_db.change_password( my->_wallet_password, new_password );
      my->_wallet_password = new_password;

      my->_dirty_accounts = true;
   } FC_CAPTURE_AND_RETHROW() }

   bool wallet::is_unlocked()const
   {
      FC_ASSERT( is_open() );
      return !wallet::is_locked();
   }

   bool wallet::is_locked()const
   {
      FC_ASSERT( is_open() );
      return my->_wallet_password == fc::sha512();
   }

   fc::optional<fc::time_point_sec> wallet::unlocked_until()const
   {
      FC_ASSERT( is_open() );
      return my->_scheduled_lock_time ? *my->_scheduled_lock_time : fc::optional<fc::time_point_sec>();
   }

   void wallet::set_setting( const string& name, const variant& value )
   {
       my->_wallet_db.store_setting(name, value);
   }

   fc::optional<variant> wallet::get_setting( const string& name )const
   {
       return my->_wallet_db.lookup_setting(name);
   }

   public_key_type wallet::create_account( const string& account_name,
                                           const variant& private_data )
   { try {
      if( !my->_blockchain->is_valid_account_name( account_name ) )
          FC_THROW_EXCEPTION( invalid_name, "Invalid account name!", ("account_name",account_name) );

      FC_ASSERT( is_open() );
      FC_ASSERT( is_unlocked() );

      const auto num_accounts_before = list_my_accounts().size();

      const auto current_account = my->_wallet_db.lookup_account( account_name );
      if( current_account.valid() )
          FC_THROW_EXCEPTION( invalid_name, "This name is already in your wallet!" );

      const auto existing_registered_account = my->_blockchain->get_account_record( account_name );
      if( existing_registered_account.valid() )
          FC_THROW_EXCEPTION( invalid_name, "This name is already registered with the blockchain!" );

      const public_key_type account_public_key = my->_wallet_db.generate_new_account( my->_wallet_password, account_name, private_data );

      if( num_accounts_before == 0 )
          set_last_scanned_block_number( my->_blockchain->get_head_block_num() );

       my->_dirty_accounts = true;

      return account_public_key;
   } FC_CAPTURE_AND_RETHROW( (account_name) ) }

   void wallet::account_set_favorite( const string& account_name,
                                      const bool is_favorite )
   { try {
       FC_ASSERT( is_open() );

       auto judged_account = my->_wallet_db.lookup_account( account_name );
       if ( !judged_account.valid() )
       {
           const auto blockchain_acct = my->_blockchain->get_account_record( account_name );
           if( !blockchain_acct.valid() )
               FC_THROW_EXCEPTION( unknown_account, "Unknown account name!" );

           add_contact_account( account_name, blockchain_acct->owner_key );
           return account_set_favorite( account_name, is_favorite );
       }
       judged_account->is_favorite = is_favorite;
       my->_wallet_db.store_account( *judged_account );
   } FC_CAPTURE_AND_RETHROW( (account_name)(is_favorite) ) }

   /**
    *  A contact is an account for which this wallet does not have the private
    *  key. If account_name is globally registered then this call will assume
    *  it is the same account and will fail if the key is not the same.
    *
    *  @param account_name - the name the account is known by to this wallet
    *  @param key - the public key that will be used for sending TITAN transactions
    *               to the account.
    */
   void wallet::add_contact_account( const string& account_name,
                                     const public_key_type& key,
                                     const variant& private_data )
   { try {
      FC_ASSERT( is_open() );

      if( !my->_blockchain->is_valid_account_name( account_name ) )
          FC_THROW_EXCEPTION( invalid_name, "Invalid account name!", ("account_name",account_name) );

      auto current_registered_account = my->_blockchain->get_account_record( account_name );
      if( current_registered_account.valid() && current_registered_account->owner_key != key )
         FC_THROW_EXCEPTION( invalid_name,
                             "Account name is already registered under a different key! Provided: ${p}, registered: ${r}",
                             ("p",key)("r",current_registered_account->active_key()) );

      if( current_registered_account.valid() && current_registered_account->is_retracted() )
          FC_CAPTURE_AND_THROW( account_retracted, (current_registered_account) );

      auto current_account = my->_wallet_db.lookup_account( account_name );
      if( current_account.valid() )
      {
         wlog( "current account is valid... ${account}", ("account",*current_account) );
         FC_ASSERT( current_account->owner_address() == address(key),
                    "Account with ${name} already exists", ("name",account_name) );

         if( current_registered_account.valid() )
         {
             blockchain::account_record& as_blockchain_account_record = *current_account;
             as_blockchain_account_record = *current_registered_account;
         }

         if( !private_data.is_null() )
            current_account->private_data = private_data;

         my->_wallet_db.store_account( *current_account );
         return;
      }
      else
      {
         current_account = my->_wallet_db.lookup_account( address( key ) );
         if( current_account.valid() )
         {
             if( current_account->name != account_name )
                 FC_THROW_EXCEPTION( duplicate_key,
                         "Provided key already belongs to another wallet account! Provided: ${p}, existing: ${e}",
                         ("p",account_name)("e",current_account->name) );

             if( current_registered_account.valid() )
             {
                 blockchain::account_record& as_blockchain_account_record = *current_account;
                 as_blockchain_account_record = *current_registered_account;
             }

             if( !private_data.is_null() )
                current_account->private_data = private_data;

             my->_wallet_db.store_account( *current_account );
             return;
         }

         if( !current_registered_account.valid() )
         {
             const time_point_sec now = blockchain::now();
             current_registered_account = account_record();
             current_registered_account->name = account_name;
             current_registered_account->owner_key = key;
             current_registered_account->set_active_key( now, key );
             current_registered_account->last_update = now;
         }

         my->_wallet_db.add_contact_account( *current_registered_account, private_data );
      }
   } FC_CAPTURE_AND_RETHROW( (account_name)(key) ) }

   // TODO: This function is sometimes used purely for error checking of the account_name; refactor
   wallet_account_record wallet::get_account( const string& account_name )const
   { try {
      FC_ASSERT( is_open() );

      if( !my->_blockchain->is_valid_account_name( account_name ) )
          FC_THROW_EXCEPTION( invalid_name, "Invalid account name!", ("account_name",account_name) );

      auto local_account = my->_wallet_db.lookup_account( account_name );
      auto chain_account = my->_blockchain->get_account_record( account_name );

      if( !local_account.valid() && !chain_account.valid() )
          FC_THROW_EXCEPTION( unknown_account, "Unknown account name!", ("account_name",account_name) );

      if( local_account.valid() && chain_account.valid() )
      {
         if( local_account->owner_key != chain_account->owner_key )
         {
            wlog( "local account is owned by someone different public key than blockchain account" );
            wdump( (local_account)(chain_account) );
         }
      }

      if( chain_account.valid() )
      {
          my->_wallet_db.store_account( *chain_account );
          local_account = my->_wallet_db.lookup_account( account_name );
      }

      FC_ASSERT( local_account.valid() );
      return *local_account;
   } FC_CAPTURE_AND_RETHROW() }

   void wallet::remove_contact_account( const string& account_name )
   { try {
      if( !my->is_unique_account(account_name) )
          FC_THROW_EXCEPTION( duplicate_account_name,
                              "Local account name conflicts with registered name. Please rename your local account first.", ("account_name",account_name) );

      const auto oaccount = my->_wallet_db.lookup_account( account_name );
      if( my->_wallet_db.has_private_key( address( oaccount->owner_key ) ) )
          FC_THROW_EXCEPTION( not_contact_account, "You can only remove contact accounts!", ("account_name",account_name) );

      my->_wallet_db.remove_contact_account( account_name );
   } FC_CAPTURE_AND_RETHROW( (account_name) ) }

   void wallet::rename_account( const string& old_account_name,
                                 const string& new_account_name )
   { try {
      FC_ASSERT( is_open() );
      if( !my->_blockchain->is_valid_account_name( old_account_name ) )
          FC_THROW_EXCEPTION( invalid_name, "Invalid old account name!", ("old_account_name",old_account_name) );
      if( !my->_blockchain->is_valid_account_name( new_account_name ) )
          FC_THROW_EXCEPTION( invalid_name, "Invalid new account name!", ("new_account_name",new_account_name) );

      optional<public_key_type> old_key;
      auto registered_account = my->_blockchain->get_account_record( old_account_name );
      bool have_registered = false;
      for( const auto& item : my->_wallet_db.get_accounts() )
      {
          const wallet_account_record& local_account = item.second;
          if( local_account.name != old_account_name )
              continue;

          if( !registered_account.valid() || registered_account->owner_key != local_account.owner_key )
          {
              old_key = local_account.owner_key;
              break;
          }

          have_registered |= registered_account.valid() && registered_account->owner_key == local_account.owner_key;
      }

      if( !old_key.valid() )
      {
          if( registered_account.valid() )
              FC_THROW_EXCEPTION( key_already_registered, "You cannot rename a registered account!" );

          FC_THROW_EXCEPTION( unknown_account, "Unknown account name!", ("old_account_name", old_account_name) );
      }

      registered_account = my->_blockchain->get_account_record( *old_key );
      if( registered_account.valid() && registered_account->name != new_account_name )
      {
          FC_THROW_EXCEPTION( key_already_registered, "That account is already registered to a different name!",
                              ("desired_name",new_account_name)("registered_name",registered_account->name) );
      }

      registered_account = my->_blockchain->get_account_record( new_account_name );
      if( registered_account.valid() )
          FC_THROW_EXCEPTION( duplicate_account_name, "Your new account name is already registered!" );

      const auto new_account = my->_wallet_db.lookup_account( new_account_name );
      if( new_account.valid() )
          FC_THROW_EXCEPTION( duplicate_account_name, "You already have the new account name in your wallet!" );

      my->_wallet_db.rename_account( *old_key, new_account_name );
      my->_dirty_accounts = true;
   } FC_CAPTURE_AND_RETHROW( (old_account_name)(new_account_name) ) }

   bool wallet::friendly_import_private_key( const private_key_type& key, const string& account_name )
   { try {
       const auto addr = address( key.get_public_key() );
       try
       {
           get_private_key( addr );
           // We already have this key and import_private_key would fail if we tried. Do nothing.
           return false;
       }
       catch( const fc::exception& )
       {
       }

       const oaccount_record blockchain_account_record = my->_blockchain->get_account_record( addr );
       if( blockchain_account_record.valid() && blockchain_account_record->name != account_name )
       { // This key exists on the blockchain and I don't have it - don't associate it with a name when you import it
           import_private_key( key, optional<string>(), false );
       }
       else
       {
           import_private_key( key, account_name, false );
       }

       return true;
   } FC_CAPTURE_AND_RETHROW( (account_name) ) }

   public_key_type wallet::import_private_key( const private_key_type& new_private_key,
                                               const optional<string>& account_name,
                                               bool create_account )
   { try {
      if( NOT is_open()     ) FC_CAPTURE_AND_THROW( wallet_closed );
      if( NOT is_unlocked() ) FC_CAPTURE_AND_THROW( wallet_locked );

      const public_key_type new_public_key = new_private_key.get_public_key();
      const address new_address = address( new_public_key );

      // Try to associate with an existing registered account
      const oaccount_record blockchain_account_record = my->_blockchain->get_account_record( new_address );
      if( blockchain_account_record.valid() )
      {
          if( account_name.valid() )
          {
              FC_ASSERT( *account_name == blockchain_account_record->name,
                         "That key already belongs to a registered account with a different name!",
                         ("blockchain_account_record",*blockchain_account_record)
                         ("account_name",*account_name) );
          }

          my->_wallet_db.store_account( *blockchain_account_record );
          my->_wallet_db.import_key( my->_wallet_password, blockchain_account_record->name, new_private_key, true );
          return new_public_key;
      }

      // Try to associate with an existing local account
      owallet_account_record account_record = my->_wallet_db.lookup_account( new_address );
      if( account_record.valid() )
      {
          if( account_name.valid() )
          {
              FC_ASSERT( *account_name == account_record->name,
                         "That key already belongs to a local account with a different name!",
                         ("account_record",*account_record)
                         ("account_name",*account_name) );
          }

          my->_wallet_db.import_key( my->_wallet_password, account_record->name, new_private_key, true );
          return new_public_key;
      }

      FC_ASSERT( account_name.valid(), "Unknown key! You must specify an account name!" );

      // Check if key is already associated with an existing local account
      const owallet_key_record key_record = my->_wallet_db.lookup_key( new_address );
      if( key_record.valid() && key_record->has_private_key() )
      {
          account_record = my->_wallet_db.lookup_account( key_record->account_address );
          if( account_record.valid() )
          {
              FC_ASSERT( *account_name == account_record->name,
                         "That key already belongs to a local account with a different name!",
                         ("account_record",*account_record)
                         ("account_name",*account_name) );
          }

          my->_wallet_db.import_key( my->_wallet_password, account_record->name, new_private_key, true );
          return new_public_key;
      }

      account_record = my->_wallet_db.lookup_account( *account_name );
      if( !account_record.valid() )
      {
          FC_ASSERT( create_account,
                     "Could not find an account with that name!",
                     ("account_name",*account_name) );

          // TODO: Replace with wallet_db.add_account
          add_contact_account( *account_name, new_public_key );
          account_record = my->_wallet_db.lookup_account( *account_name );
          FC_ASSERT( account_record.valid(), "Error creating new account!" );
      }

      my->_wallet_db.import_key( my->_wallet_password, account_record->name, new_private_key, true );
      return new_public_key;
   } FC_CAPTURE_AND_RETHROW( (account_name)(create_account) ) }

   public_key_type wallet::import_wif_private_key( const string& wif_key,
                                                   const optional<string>& account_name,
                                                   bool create_account )
   { try {
      FC_ASSERT( is_open() );
      FC_ASSERT( is_unlocked() );

      auto key = bts::utilities::wif_to_key( wif_key );
      if( key.valid() )
      {
          import_private_key( *key, account_name, create_account );
          my->_dirty_accounts = true;
          return key->get_public_key();
      }

      FC_ASSERT( false, "Error parsing WIF private key" );

   } FC_CAPTURE_AND_RETHROW( (account_name) ) }

   void wallet::start_scan( const uint32_t start_block_num, const uint32_t limit )
   { try {
       if( NOT is_open()     ) FC_CAPTURE_AND_THROW( wallet_closed );
       if( NOT is_unlocked() ) FC_CAPTURE_AND_THROW( wallet_locked );

       if( my->_scan_in_progress.valid() && !my->_scan_in_progress.ready() )
           return;

       if( !get_transaction_scanning() )
       {
           my->_scan_progress = 1;
           ulog( "Wallet transaction scanning is disabled!" );
           return;
       }

       const auto scan_chain_task = [=]() { my->start_scan_task( start_block_num, limit ); };
       my->_scan_in_progress = fc::async( scan_chain_task, "scan_chain_task" );

       my->_scan_in_progress.on_complete( []( fc::exception_ptr ep )
       { if( ep ) elog( "Error during scanning: ${e}", ("e",ep->to_detail_string()) ); } );
   } FC_CAPTURE_AND_RETHROW( (start_block_num)(limit) ) }

   void wallet::cancel_scan()
   { try {
       try
       {
           ilog( "Canceling wallet scan_chain_task..." );
           my->_scan_in_progress.cancel_and_wait( "wallet::cancel_scan()" );
           ilog( "Wallet scan_chain_task canceled..." );
       }
       catch( const fc::exception& e )
       {
           wlog( "Unexpected exception from wallet's scan_chain_task: ${e}", ("e",e.to_detail_string()) );
       }
       catch( ... )
       {
           wlog( "Unexpected exception from wallet's scan_chain_task" );
       }
       my->_scan_progress = 1;
   } FC_CAPTURE_AND_RETHROW() }

   vote_summary wallet::get_vote_status( const string& account_name )
   {
       uint64_t total_possible = 0;
       uint64_t total = 0;
       auto summary = vote_summary();
       summary.up_to_date_with_recommendation = true;

       auto my_slate = my->get_delegate_slate( vote_recommended );

       const account_balance_record_summary_type items = get_spendable_account_balance_records( account_name );
       for( const auto& item : items )
       {
           const auto& records = item.second;
           for( const auto& record : records )
           {
               if( record.asset_id() != 0 )
                   continue;
               if( record.slate_id() != my_slate.id() && record.balance > 1 * BTS_BLOCKCHAIN_PRECISION ) // ignore dust
                   summary.up_to_date_with_recommendation = false;
               auto oslate = my->_blockchain->get_slate_record( record.slate_id() );
               if( oslate.valid() )
                   total += record.get_spendable_balance( my->_blockchain->now() ).amount * oslate->slate.size();
               total_possible += record.get_spendable_balance( my->_blockchain->now() ).amount * BTS_BLOCKCHAIN_MAX_SLATE_SIZE;
           }
       }

       if( total_possible == 0 )
           summary.utilization = 0;
       else
           summary.utilization = float(total) / float(total_possible);

       summary.negative_utilization = 0;

       return summary;
   }

   private_key_type wallet::get_private_key( const address& addr )const
   { try {
      FC_ASSERT( is_open() );
      FC_ASSERT( is_unlocked() );

      auto key = my->_wallet_db.lookup_key( addr );
      FC_ASSERT( key.valid() );
      FC_ASSERT( key->has_private_key() );
      return key->decrypt_private_key( my->_wallet_password );
   } FC_CAPTURE_AND_RETHROW( (addr) ) }

   public_key_type  wallet::get_public_key( const address& addr) const
   {
       FC_ASSERT( is_open() );
       auto key = my->_wallet_db.lookup_key( addr );
       FC_ASSERT( key.valid(), "No known key for this address." );
       return key->public_key;
   }

   void wallet::set_delegate_block_production( const string& delegate_name, bool enabled )
   {
      FC_ASSERT( is_open() );
      std::vector<wallet_account_record> delegate_records;
      const auto empty_before = get_my_delegates( enabled_delegate_status ).empty();

      if( delegate_name != "ALL" )
      {
          if( !my->_blockchain->is_valid_account_name( delegate_name ) )
              FC_THROW_EXCEPTION( invalid_name, "Invalid delegate name!", ("delegate_name",delegate_name) );

          auto delegate_record = get_account( delegate_name );
          FC_ASSERT( delegate_record.is_delegate(), "${name} is not a delegate.", ("name", delegate_name) );
          auto key = my->_wallet_db.lookup_key( delegate_record.signing_address() );
          FC_ASSERT( key.valid() && key->has_private_key(), "Unable to find private key for ${name}.", ("name", delegate_name) );
          delegate_records.push_back( delegate_record );
      }
      else
      {
          delegate_records = get_my_delegates( any_delegate_status );
      }

      for( auto& delegate_record : delegate_records )
      {
          delegate_record.block_production_enabled = enabled;
          my->_wallet_db.store_account( delegate_record );
      }

      const auto empty_after = get_my_delegates( enabled_delegate_status ).empty();

      if( empty_before == empty_after )
      {
          return;
      }
      else if( empty_before )
      {
          ulog( "Wallet transaction scanning has been automatically disabled due to enabled delegates!" );
          set_transaction_scanning( false );
      }
      else /* if( empty_after ) */
      {
          ulog( "Wallet transaction scanning has been automatically re-enabled!" );
          set_transaction_scanning( true );
      }
   }

   vector<wallet_account_record> wallet::get_my_delegates( uint32_t delegates_to_retrieve )const
   {
      FC_ASSERT( is_open() );
      vector<wallet_account_record> delegate_records;
      const auto& account_records = list_my_accounts();
      for( const auto& account_record : account_records )
      {
          if( !account_record.is_delegate() ) continue;
          if( delegates_to_retrieve & enabled_delegate_status && !account_record.block_production_enabled ) continue;
          if( delegates_to_retrieve & disabled_delegate_status && account_record.block_production_enabled ) continue;
          if( delegates_to_retrieve & active_delegate_status && !my->_blockchain->is_active_delegate( account_record.id ) ) continue;
          if( delegates_to_retrieve & inactive_delegate_status && my->_blockchain->is_active_delegate( account_record.id ) ) continue;
          delegate_records.push_back( account_record );
      }
      return delegate_records;
   }

   optional<time_point_sec> wallet::get_next_producible_block_timestamp( const vector<wallet_account_record>& delegate_records )const
   { try {
      if( !is_open() || is_locked() ) return optional<time_point_sec>();

      vector<account_id_type> delegate_ids;
      delegate_ids.reserve( delegate_records.size() );
      for( const auto& delegate_record : delegate_records )
          delegate_ids.push_back( delegate_record.id );

      return my->_blockchain->get_next_producible_block_timestamp( delegate_ids );
   } FC_CAPTURE_AND_RETHROW() }


    fc::ecc::compact_signature wallet::sign_hash(const string& signer, const fc::sha256& hash )const
    {
        try {
           auto key = public_key_type( signer );
           auto privkey = get_private_key( address( key ) );
           return privkey.sign_compact( hash );
       }
       catch (...)
       {
           try {
               auto addr = address( signer );
               auto privkey = get_private_key( addr );
               return privkey.sign_compact( hash );
           }
           catch (...)
           {
               return get_active_private_key(signer).sign_compact(hash);
           }
       }
    }

   void wallet::sign_block( signed_block_header& header )const
   { try {
      if( NOT is_open()     ) FC_CAPTURE_AND_THROW( wallet_closed );
      if( NOT is_unlocked() ) FC_CAPTURE_AND_THROW( wallet_locked );

      const vector<account_id_type>& active_delegate_ids = my->_blockchain->get_active_delegates();
      const account_record delegate_record = my->_blockchain->get_slot_signee( header.timestamp, active_delegate_ids );
      FC_ASSERT( delegate_record.is_delegate() );

      const public_key_type public_signing_key = delegate_record.signing_key();
      const private_key_type private_signing_key = get_private_key( address( public_signing_key ) );

      FC_ASSERT( delegate_record.delegate_info.valid() );
      const uint32_t last_produced_block_num = delegate_record.delegate_info->last_block_num_produced;

      const optional<secret_hash_type>& prev_secret_hash = delegate_record.delegate_info->next_secret_hash;
      if( prev_secret_hash.valid() )
      {
          FC_ASSERT( !delegate_record.delegate_info->signing_key_history.empty() );
          const map<uint32_t, public_key_type>& signing_key_history = delegate_record.delegate_info->signing_key_history;
          const uint32_t last_signing_key_change_block_num = signing_key_history.crbegin()->first;

          if( last_produced_block_num > last_signing_key_change_block_num )
          {
              header.previous_secret = my->get_secret( last_produced_block_num, private_signing_key );
          }
          else
          {
              // We need to use the old key to reveal the previous secret
              FC_ASSERT( signing_key_history.size() >= 2 );
              auto iter = signing_key_history.crbegin();
              ++iter;

              const public_key_type& prev_public_signing_key = iter->second;
              const private_key_type prev_private_signing_key = get_private_key( address( prev_public_signing_key ) );

              header.previous_secret = my->get_secret( last_produced_block_num, prev_private_signing_key );
          }

          FC_ASSERT( fc::ripemd160::hash( header.previous_secret ) == *prev_secret_hash );
      }

      header.next_secret_hash = fc::ripemd160::hash( my->get_secret( header.block_num, private_signing_key ) );
      header.sign( private_signing_key );

      FC_ASSERT( header.validate_signee( public_signing_key ) );
   } FC_CAPTURE_AND_RETHROW( (header) ) }

   std::shared_ptr<transaction_builder> wallet::create_transaction_builder()
   { try {
       return std::make_shared<transaction_builder>( my.get() );
   } FC_CAPTURE_AND_RETHROW() }

   std::shared_ptr<transaction_builder> wallet::create_transaction_builder(const transaction_builder& old_builder)
   { try {
       auto builder = std::make_shared<transaction_builder>( old_builder, my.get() );
       return builder;
   } FC_CAPTURE_AND_RETHROW() }

   std::shared_ptr<transaction_builder> wallet::create_transaction_builder_from_file(const string& old_builder_path)
   { try {
       auto path = old_builder_path;
       if( path == "" )
       {
            path = (get_data_directory() / "trx").string() + "/latest.trx";
       }
       auto old_builder = fc::json::from_file(path).as<transaction_builder>();
       auto builder = std::make_shared<transaction_builder>( old_builder, my.get() );
       return builder;
   } FC_CAPTURE_AND_RETHROW() }


   vector<std::pair<string, wallet_transaction_record>> wallet::publish_feeds_multi_experimental(
           map<string,double> amount_per_xts, // map symbol to amount per xts
           bool sign )
   {
	   vector<std::pair<string, wallet_transaction_record>> result;
	   const vector<wallet_account_record> accounts = this->list_my_accounts();

	   for( const auto& acct : accounts )
	   {
		   auto current_account = my->_blockchain->get_account_record( acct.name );
		   if( !current_account.valid() || current_account->is_retracted() || !current_account->is_delegate() )
			   continue;
	       wallet_transaction_record tr = this->publish_feeds(
	           acct.name,
	           amount_per_xts,
	           sign);
		   std::pair<string, wallet_transaction_record> p(acct.name, tr);
		   result.push_back(p);
	   }
       return result;
   }

   wallet_transaction_record wallet::publish_feeds(
           const string& account_to_publish_under,
           map<string,double> amount_per_xts, // map symbol to amount per xts
           bool sign )
   { try {
      FC_ASSERT( is_open() );
      FC_ASSERT( is_unlocked() );

      if( !my->is_receive_account( account_to_publish_under ) )
          FC_THROW_EXCEPTION( unknown_receive_account, "You cannot publish from this account!",
                              ("delegate_account",account_to_publish_under) );

      for( auto item : amount_per_xts )
      {
         if( item.second < 0 )
             FC_THROW_EXCEPTION( invalid_price, "Invalid price!", ("amount_per_xts",item) );
      }

      signed_transaction     trx;
      unordered_set<address> required_signatures;

      trx.expiration = blockchain::now() + get_transaction_expiration();

      auto current_account = my->_blockchain->get_account_record( account_to_publish_under );
      FC_ASSERT( current_account.valid() );
      auto payer_public_key = get_owner_public_key( account_to_publish_under );

      for( const auto& item : amount_per_xts )
      {
         auto quote_asset_record = my->_blockchain->get_asset_record( item.first );
         auto base_asset_record  = my->_blockchain->get_asset_record( BTS_BLOCKCHAIN_SYMBOL );
         FC_ASSERT( quote_asset_record.valid(), "Invalid Symbol '${symbol}'", ("symbol",item.first) );


         asset price_shares( item.second *  quote_asset_record->precision, quote_asset_record->id );
         asset base_one_quantity( base_asset_record->precision, 0 );

         // auto quote_price_shares = price_shares / base_one_quantity;
         price quote_price_shares( (item.second * quote_asset_record->precision) / base_asset_record->precision, quote_asset_record->id, base_asset_record->id );

         if( item.second > 0 )
         {
            trx.publish_feed( my->_blockchain->get_asset_id( item.first ),
                              current_account->id, fc::variant( quote_price_shares )  );
         }
         else
         {
            trx.publish_feed( my->_blockchain->get_asset_id( item.first ),
                              current_account->id, fc::variant()  );
         }
      }

      auto required_fees = get_transaction_fee();

      if( required_fees.amount < current_account->delegate_pay_balance() )
      {
        // withdraw delegate pay...
        trx.withdraw_pay( current_account->id, required_fees.amount );
      }
      else
      {
         my->withdraw_to_transaction( required_fees,
                                      account_to_publish_under,
                                      trx,
                                      required_signatures );
      }
      required_signatures.insert( current_account->active_key() );

      auto entry = ledger_entry();
      entry.from_account = payer_public_key;
      entry.to_account = payer_public_key;
      entry.memo = "publish price feeds";

      auto record = wallet_transaction_record();
      record.ledger_entries.push_back( entry );
      record.fee = required_fees;

      if( sign )
          my->sign_transaction( trx, required_signatures );

      record.trx = trx;
      return record;
   } FC_CAPTURE_AND_RETHROW( (account_to_publish_under)(amount_per_xts) ) }

   wallet_transaction_record wallet::publish_price(
           const string& account_to_publish_under,
           double amount_per_xts,
           const string& amount_asset_symbol,
           bool force_settle,
           bool sign )
   { try {
      FC_ASSERT( is_open() );
      FC_ASSERT( is_unlocked() );

      if( !my->is_receive_account( account_to_publish_under ) )
          FC_THROW_EXCEPTION( unknown_receive_account, "You cannot publish from this account!",
                              ("delegate_account",account_to_publish_under) );

      if( amount_per_xts < 0 )
          FC_THROW_EXCEPTION( invalid_price, "Invalid price!", ("amount_per_xts",amount_per_xts) );

      signed_transaction     trx;
      unordered_set<address> required_signatures;

      trx.expiration = blockchain::now() + get_transaction_expiration();

      auto current_account = my->_blockchain->get_account_record( account_to_publish_under );
      FC_ASSERT( current_account.valid() );
      auto payer_public_key = get_owner_public_key( account_to_publish_under );

      auto quote_asset_record = my->_blockchain->get_asset_record( amount_asset_symbol );
      auto base_asset_record  = my->_blockchain->get_asset_record( BTS_BLOCKCHAIN_SYMBOL );
      FC_ASSERT( base_asset_record.valid() );
      FC_ASSERT( quote_asset_record.valid() );

      feed_price quote_price_shares;
      const double ratio = (amount_per_xts * quote_asset_record->precision) / base_asset_record->precision;
      price& standard_price = quote_price_shares;
      standard_price = price( ratio, quote_asset_record->id, base_asset_record->id );
      quote_price_shares.force_settle = force_settle;

      idump( (quote_price_shares) );
      if( amount_per_xts > 0 )
      {
         trx.publish_feed( my->_blockchain->get_asset_id( amount_asset_symbol ),
                           current_account->id, fc::variant( price( quote_price_shares ) )  );
      }
      else
      {
         trx.publish_feed( my->_blockchain->get_asset_id( amount_asset_symbol ),
                           current_account->id, fc::variant()  );
      }

      auto required_fees = get_transaction_fee();

      if( required_fees.amount <  current_account->delegate_pay_balance() )
      {
        // withdraw delegate pay...
        trx.withdraw_pay( current_account->id, required_fees.amount );
      }
      else
      {
         my->withdraw_to_transaction( required_fees,
                                      account_to_publish_under,
                                      trx,
                                      required_signatures );
      }
      required_signatures.insert( current_account->active_key() );

      auto entry = ledger_entry();
      entry.from_account = payer_public_key;
      entry.to_account = payer_public_key;
      entry.memo = "publish price " + my->_blockchain->to_pretty_price( quote_price_shares );

      auto record = wallet_transaction_record();
      record.ledger_entries.push_back( entry );
      record.fee = required_fees;

      if( sign )
          my->sign_transaction( trx, required_signatures );

      record.trx = trx;
      return record;
   } FC_CAPTURE_AND_RETHROW( (account_to_publish_under)(amount_per_xts)(amount_asset_symbol)(sign) ) }

   // TODO: Refactor publish_{slate|version} are exactly the same
   wallet_transaction_record wallet::publish_slate(
           const string& account_to_publish_under,
           const string& account_to_pay_with,
           bool sign )
   { try {
      FC_ASSERT( is_open() );
      FC_ASSERT( is_unlocked() );

      string paying_account = account_to_pay_with;
      if( paying_account.empty() )
        paying_account = account_to_publish_under;

      if( !my->is_receive_account( paying_account ) )
          FC_THROW_EXCEPTION( unknown_receive_account, "Unknown paying account!", ("paying_account", paying_account) );

      auto current_account = my->_blockchain->get_account_record( account_to_publish_under );
      if( !current_account.valid() )
          FC_THROW_EXCEPTION( unknown_account, "Unknown publishing account!", ("account_to_publish_under", account_to_publish_under) );

      signed_transaction     trx;
      unordered_set<address> required_signatures;

      trx.expiration = blockchain::now() + get_transaction_expiration();

      const auto payer_public_key = get_owner_public_key( paying_account );

      const auto slate_id = my->set_delegate_slate( trx, vote_all );
      if( slate_id == 0 )
          FC_THROW_EXCEPTION( invalid_slate, "Cannot publish the null slate!" );

      fc::mutable_variant_object public_data;
      if( current_account->public_data.is_object() )
          public_data = current_account->public_data.get_object();

      public_data[ "slate_id" ] = slate_id;

      trx.update_account( current_account->id,
                          current_account->delegate_pay_rate(),
                          fc::variant_object( public_data ),
                          optional<public_key_type>() );
      my->authorize_update( required_signatures, current_account );

      const auto required_fees = get_transaction_fee();

      if( current_account->is_delegate() && required_fees.amount < current_account->delegate_pay_balance() )
      {
        // withdraw delegate pay...
        trx.withdraw_pay( current_account->id, required_fees.amount );
        required_signatures.insert( current_account->active_key() );
      }
      else
      {
         my->withdraw_to_transaction( required_fees,
                                      paying_account,
                                      trx,
                                      required_signatures );
      }

      auto entry = ledger_entry();
      entry.from_account = payer_public_key;
      entry.to_account = payer_public_key;
      entry.memo = "publish slate " + fc::variant(slate_id).as_string();

      auto record = wallet_transaction_record();
      record.ledger_entries.push_back( entry );
      record.fee = required_fees;

      if( sign )
          my->sign_transaction( trx, required_signatures );

      record.trx = trx;
      return record;
   } FC_CAPTURE_AND_RETHROW( (account_to_publish_under)(account_to_pay_with)(sign) ) }

   // TODO: Refactor publish_{slate|version} are exactly the same
   wallet_transaction_record wallet::publish_version(
           const string& account_to_publish_under,
           const string& account_to_pay_with,
           bool sign )
   { try {
      FC_ASSERT( is_open() );
      FC_ASSERT( is_unlocked() );

      string paying_account = account_to_pay_with;
      if( paying_account.empty() )
        paying_account = account_to_publish_under;

      if( !my->is_receive_account( paying_account ) )
          FC_THROW_EXCEPTION( unknown_receive_account, "Unknown paying account!", ("paying_account", paying_account) );

      auto current_account = my->_blockchain->get_account_record( account_to_publish_under );
      if( !current_account.valid() )
          FC_THROW_EXCEPTION( unknown_account, "Unknown publishing account!", ("account_to_publish_under", account_to_publish_under) );

      signed_transaction     trx;
      unordered_set<address> required_signatures;

      trx.expiration = blockchain::now() + get_transaction_expiration();

      const auto payer_public_key = get_owner_public_key( paying_account );

      fc::mutable_variant_object public_data;
      if( current_account->public_data.is_object() )
          public_data = current_account->public_data.get_object();

      const auto version = bts::client::version_info()["client_version"].as_string();
      public_data[ "version" ] = version;

      trx.update_account( current_account->id,
                          current_account->delegate_pay_rate(),
                          fc::variant_object( public_data ),
                          optional<public_key_type>() );
      my->authorize_update( required_signatures, current_account );

      const auto required_fees = get_transaction_fee();

      if( current_account->is_delegate() && required_fees.amount < current_account->delegate_pay_balance() )
      {
        // withdraw delegate pay...
        trx.withdraw_pay( current_account->id, required_fees.amount );
        required_signatures.insert( current_account->active_key() );
      }
      else
      {
         my->withdraw_to_transaction( required_fees,
                                      paying_account,
                                      trx,
                                      required_signatures );
      }

      auto entry = ledger_entry();
      entry.from_account = payer_public_key;
      entry.to_account = payer_public_key;
      entry.memo = "publish version " + version;

      auto record = wallet_transaction_record();
      record.ledger_entries.push_back( entry );
      record.fee = required_fees;

      if( sign )
          my->sign_transaction( trx, required_signatures );

      record.trx = trx;
      return record;
   } FC_CAPTURE_AND_RETHROW( (account_to_publish_under)(account_to_pay_with)(sign) ) }

   wallet_transaction_record wallet::collect_account_balances( const string& account_name,
                                                               const function<bool( const balance_record& )> filter,
                                                               const string& memo_message, bool sign )
   { try {
      if( NOT is_open()     ) FC_CAPTURE_AND_THROW( wallet_closed );
      if( NOT is_unlocked() ) FC_CAPTURE_AND_THROW( wallet_locked );

      const owallet_account_record account_record = my->_wallet_db.lookup_account( account_name );
      if( !account_record.valid() || !account_record->is_my_account )
          FC_CAPTURE_AND_THROW( unknown_receive_account, (account_name) );

      account_balance_record_summary_type balance_records;
      const time_point_sec now = my->_blockchain->get_pending_state()->now();

      const auto scan_balance = [&]( const balance_id_type& id, const balance_record& record )
      {
          if( !filter( record ) ) return;

          const asset balance = record.get_spendable_balance( now );
          if( balance.amount == 0 ) return;

          const optional<address> owner = record.owner();
          if( !owner.valid() ) return;

          const owallet_key_record key_record = my->_wallet_db.lookup_key( *owner );
          if( !key_record.valid() || !key_record->has_private_key() ) return;

          const owallet_account_record account_record = my->_wallet_db.lookup_account( key_record->account_address );
          if( !account_record.valid() || account_record->name != account_name ) return;

          balance_records[ account_name ].push_back( record );
      };

      scan_balances( scan_balance );

      if( balance_records.find( account_name ) == balance_records.end() )
          FC_CAPTURE_AND_THROW( insufficient_funds, (account_name) );

      signed_transaction trx;
      trx.expiration = blockchain::now() + get_transaction_expiration();
      unordered_set<address> required_signatures;

      asset total_balance;
      for( const balance_record& record : balance_records.at( account_name ) )
      {
          const asset balance = record.get_spendable_balance( my->_blockchain->get_pending_state()->now() );
          trx.withdraw( record.id(), balance.amount );
          const auto owner = record.owner();
          if( !owner.valid() ) continue;
          required_signatures.insert( *owner );
          total_balance += balance;
      }

      trx.deposit_to_account( account_record->active_key(),
                              total_balance - get_transaction_fee(),
                              get_private_key( account_record->active_key() ),
                              memo_message,
                              account_record->active_key(),
                              my->get_new_private_key( account_name ),
                              from_memo,
                              !account_record->is_public_account()
                              );

      auto entry = ledger_entry();
      entry.from_account = account_record->owner_key;
      entry.to_account = account_record->owner_key;
      entry.amount = total_balance - get_transaction_fee();
      entry.memo = memo_message;

      auto record = wallet_transaction_record();
      record.ledger_entries.push_back( entry );
      record.fee = get_transaction_fee();

      if( sign )
          my->sign_transaction( trx, required_signatures );

      record.trx = trx;
      return record;
   } FC_CAPTURE_AND_RETHROW( (account_name)(sign) ) }

   transaction_builder wallet::set_vote_info(
           const balance_id_type& balance_id,
           const address& voter_address,
           vote_strategy strategy )
   { try {
      FC_ASSERT( is_open() );
      FC_ASSERT( is_unlocked() );

      auto builder = create_transaction_builder();
      const auto required_fees = get_transaction_fee();
      auto balance = my->_blockchain->get_balance_record( balance_id );
      FC_ASSERT( balance.valid(), "No such balance!" );

      signed_transaction trx;
      trx.expiration = blockchain::now() + get_transaction_expiration();

      trx.update_balance_vote( balance_id, voter_address );
      my->set_delegate_slate( trx, strategy );

      if( balance->restricted_owner == voter_address ) // not an owner update
      {
          builder->required_signatures.insert( voter_address );
      }
      else
      {
          const auto owner = balance->owner();
          FC_ASSERT( owner.valid() );
          builder->required_signatures.insert( *owner );
      }

      auto entry = ledger_entry();
      entry.memo = "Set balance vote info";
      auto record = wallet_transaction_record();
      record.ledger_entries.push_back( entry );
      record.fee = required_fees;

      record.trx = trx;
      builder->transaction_record = record;
      return *builder;
   } FC_CAPTURE_AND_RETHROW( (balance_id)(voter_address)(strategy) ) }


   wallet_transaction_record wallet::update_signing_key(
           const string& authorizing_account_name,
           const string& delegate_name,
           const public_key_type& signing_key,
           bool sign
           )
   { try {
      if( NOT is_open()     ) FC_CAPTURE_AND_THROW( wallet_closed );
      if( NOT is_unlocked() ) FC_CAPTURE_AND_THROW( wallet_locked );

      transaction_builder_ptr builder = create_transaction_builder();
      builder->update_signing_key( authorizing_account_name, delegate_name, signing_key );
      builder->finalize();

      if( sign )
      {
          my->_dirty_accounts = true;
          return builder->sign();
      }
      return builder->transaction_record;
   } FC_CAPTURE_AND_RETHROW( (authorizing_account_name)(delegate_name)(signing_key)(sign) ) }

   void wallet::repair_records( const optional<string>& collecting_account_name )
   { try {
       if( NOT is_open()     ) FC_CAPTURE_AND_THROW( wallet_closed );
       if( NOT is_unlocked() ) FC_CAPTURE_AND_THROW( wallet_locked );

       ulog( "Repairing wallet records. This may take a while..." );
       my->_wallet_db.repair_records( my->_wallet_password );

       if( !collecting_account_name.valid() )
           return;

       const owallet_account_record account_record = my->_wallet_db.lookup_account( *collecting_account_name );
       FC_ASSERT( account_record.valid(), "Cannot find a local account with that name!",
                  ("collecting_account_name",*collecting_account_name) );

       map<string, vector<balance_record>> items = get_spendable_account_balance_records();
       for( const auto& item : items )
       {
           const auto& name = item.first;
           const auto& records = item.second;

           if( name.find( BTS_ADDRESS_PREFIX ) != 0 )
               continue;

           for( const auto& record : records )
           {
               const auto owner = record.owner();
               if( !owner.valid() ) continue;
               owallet_key_record key_record = my->_wallet_db.lookup_key( *owner );
               if( key_record.valid() )
               {
                   key_record->account_address = account_record->owner_address();
                   my->_wallet_db.store_key( *key_record );
               }
           }
       }
   } FC_CAPTURE_AND_RETHROW( (collecting_account_name) ) }

   uint32_t wallet::regenerate_keys( const string& account_name, uint32_t num_keys_to_regenerate )
   { try {
       FC_ASSERT( is_open() );
       FC_ASSERT( is_unlocked() );
       FC_ASSERT( num_keys_to_regenerate > 0 );

       owallet_account_record account_record = my->_wallet_db.lookup_account( account_name );
       FC_ASSERT( account_record.valid() );

       // Update local account records with latest global state
       my->scan_accounts();

       ulog( "This may take a while..." );
       uint32_t total_regenerated_key_count = 0;

       // Regenerate wallet child keys
       ulog( "Regenerating wallet child keys and importing into account: ${name}", ("name",account_name) );
       uint32_t key_index = 0;
       for( ; key_index < num_keys_to_regenerate; ++key_index )
       {
           fc::oexception regenerate_key_error;
           try
           {
               const private_key_type private_key = my->_wallet_db.get_wallet_child_key( my->_wallet_password, key_index );
               import_private_key( private_key, account_name );
               ++total_regenerated_key_count;
           }
           catch( const fc::exception& e )
           {
               regenerate_key_error = e;
           }

           if( regenerate_key_error.valid() )
               ulog( "${e}", ("e",regenerate_key_error->to_detail_string()) );
       }

       // Update wallet last used child key index
       my->_wallet_db.set_last_wallet_child_key_index( std::max( my->_wallet_db.get_last_wallet_child_key_index(), key_index - 1 ) );

       // Regenerate v1 account child keys
       ulog( "Regenerating type 1 account child keys for account: ${name}", ("name",account_name) );
       uint32_t seq_num = 0;
       for( ; seq_num < num_keys_to_regenerate; ++seq_num )
       {
           fc::oexception regenerate_key_error;
           try
           {
               const private_key_type private_key = my->_wallet_db.get_account_child_key_v1( my->_wallet_password,
                                                                                             account_record->owner_address(), seq_num );
               import_private_key( private_key, account_name );
               ++total_regenerated_key_count;
           }
           catch( const fc::exception& e )
           {
               regenerate_key_error = e;
           }

           if( regenerate_key_error.valid() )
               ulog( "${e}", ("e",regenerate_key_error->to_detail_string()) );
       }

       // Regenerate v2 account child keys
       const owallet_key_record key_record = my->_wallet_db.lookup_key( address( account_record->active_key() ) );
       if( key_record.valid() && key_record->has_private_key() )
       {
           ulog( "Regenerating type 2 account child keys for account: ${name}", ("name",account_name) );
           const private_key_type active_private_key = key_record->decrypt_private_key( my->_wallet_password );
           seq_num = 0;
           for( ; seq_num < num_keys_to_regenerate; ++seq_num )
           {
               fc::oexception regenerate_key_error;
               try
               {
                   const private_key_type private_key = my->_wallet_db.get_account_child_key( active_private_key, seq_num );
                   import_private_key( private_key, account_name );
                   ++total_regenerated_key_count;
               }
               catch( const fc::exception& e )
               {
                   regenerate_key_error = e;
               }

               if( regenerate_key_error.valid() )
                   ulog( "${e}", ("e",regenerate_key_error->to_detail_string()) );
           }
       }

       // Update account last used key sequence number
       account_record->last_used_gen_sequence = std::max( account_record->last_used_gen_sequence, seq_num - 1 );
       my->_wallet_db.store_account( *account_record );

       ulog( "Successfully generated ${n} keys.", ("n",total_regenerated_key_count) );

       my->_dirty_balances = true;
       my->_dirty_accounts = true;

       if( total_regenerated_key_count > 0 )
           start_scan( 0, -1 );

       ulog( "Key regeneration may leave the wallet in an inconsistent state." );
       ulog( "It is recommended to create a new wallet and transfer all funds." );
       return total_regenerated_key_count;
   } FC_CAPTURE_AND_RETHROW() }

   int32_t wallet::recover_accounts( int32_t number_of_accounts, int32_t max_number_of_attempts )
   {
     FC_ASSERT( is_open() );
     FC_ASSERT( is_unlocked() );

     int attempts = 0;
     int recoveries = 0;

     uint32_t key_index = my->_wallet_db.get_last_wallet_child_key_index() + 1;
     while( recoveries < number_of_accounts && attempts++ < max_number_of_attempts )
     {
        const private_key_type new_priv_key = my->_wallet_db.get_wallet_child_key( my->_wallet_password, key_index );
        fc::ecc::public_key new_pub_key = new_priv_key.get_public_key();
        auto recovered_account = my->_blockchain->get_account_record(new_pub_key);

        if( recovered_account.valid() )
        {
          my->_wallet_db.set_last_wallet_child_key_index( key_index );
          import_private_key(new_priv_key, recovered_account->name, true);
          ++recoveries;
        }

        ++key_index;
     }

     my->_dirty_accounts = true;

     if( recoveries )
       start_scan( 0, -1 );
     return recoveries;
   }

   // TODO: Rename to recover_titan_transaction_info
   wallet_transaction_record wallet::recover_transaction( const string& transaction_id_prefix, const string& recipient_account )
   { try {
       FC_ASSERT( is_open() );
       FC_ASSERT( is_unlocked() );

       auto transaction_record = get_transaction( transaction_id_prefix );

       /* Only support standard transfers for now */
       FC_ASSERT( transaction_record.ledger_entries.size() == 1 );
       auto ledger_entry = transaction_record.ledger_entries.front();

       /* In case the transaction was not saved in the record */
       if( transaction_record.trx.operations.empty() )
       {
           const auto blockchain_transaction_record = my->_blockchain->get_transaction( transaction_record.record_id, true );
           FC_ASSERT( blockchain_transaction_record.valid() );
           transaction_record.trx = blockchain_transaction_record->trx;
       }

       /* Only support a single deposit */
       deposit_operation deposit_op;
       bool has_deposit = false;
       for( const auto& op : transaction_record.trx.operations )
       {
           switch( operation_type_enum( op.type ) )
           {
               case deposit_op_type:
                   FC_ASSERT( !has_deposit );
                   deposit_op = op.as<deposit_operation>();
                   has_deposit = true;
                   break;
               default:
                   break;
           }
       }
       FC_ASSERT( has_deposit );

       /* Only support standard withdraw by signature condition with memo */
       FC_ASSERT( withdraw_condition_types( deposit_op.condition.type ) == withdraw_signature_type );
       const auto withdraw_condition = deposit_op.condition.as<withdraw_with_signature>();
       FC_ASSERT( withdraw_condition.memo.valid() );

       /* We had to have stored the one-time key */
       const auto key_record = my->_wallet_db.lookup_key( withdraw_condition.memo->one_time_key );
       FC_ASSERT( key_record.valid() && key_record->has_private_key() );
       const auto private_key = key_record->decrypt_private_key( my->_wallet_password );

       /* Get shared secret and check memo decryption */
       bool found_recipient = false;
       public_key_type recipient_public_key;
       extended_memo_data memo;
       if( !recipient_account.empty() )
       {
           recipient_public_key = get_owner_public_key( recipient_account );
           const auto shared_secret = private_key.get_shared_secret( recipient_public_key );
           memo = withdraw_condition.decrypt_memo_data( shared_secret );
           found_recipient = true;
       }
       else
       {
           const auto check_account = [&]( const account_record& record )
           {
               try
               {
                   recipient_public_key = record.owner_key;
                   // TODO: Need to check active keys as well as owner key
                   const auto shared_secret = private_key.get_shared_secret( recipient_public_key );
                   memo = withdraw_condition.decrypt_memo_data( shared_secret );
               }
               catch( ... )
               {
                   return;
               }
               found_recipient = true;
               FC_ASSERT( false ); /* Kill scanning since we found it */
           };

           try
           {
               my->_blockchain->scan_unordered_accounts( check_account );
           }
           catch( ... )
           {
           }
       }
       FC_ASSERT( found_recipient );

       /* Update ledger entry with recipient and memo info */
       ledger_entry.to_account = recipient_public_key;
       ledger_entry.memo = memo.get_message();
       transaction_record.ledger_entries[ 0 ] = ledger_entry;
       my->_wallet_db.store_transaction( transaction_record );

       return transaction_record;
   } FC_CAPTURE_AND_RETHROW() }

   optional<variant_object> wallet::verify_titan_deposit( const string& transaction_id_prefix )
   { try {
       FC_ASSERT( is_open() );
       FC_ASSERT( is_unlocked() );

       // TODO: Separate this finding logic
       if( transaction_id_prefix.size() < 8 || transaction_id_prefix.size() > string( transaction_id_type() ).size() )
           FC_THROW_EXCEPTION( invalid_transaction_id, "Invalid transaction id!", ("transaction_id_prefix",transaction_id_prefix) );

       const transaction_id_type transaction_id = variant( transaction_id_prefix ).as<transaction_id_type>();
       const otransaction_record transaction_record = my->_blockchain->get_transaction( transaction_id, false );
       if( !transaction_record.valid() )
           FC_THROW_EXCEPTION( transaction_not_found, "Transaction not found!", ("transaction_id_prefix",transaction_id_prefix) );

       /* Only support a single deposit */
       deposit_operation deposit_op;
       bool has_deposit = false;
       for( const auto& op : transaction_record->trx.operations )
       {
           switch( operation_type_enum( op.type ) )
           {
               case deposit_op_type:
                   FC_ASSERT( !has_deposit );
                   deposit_op = op.as<deposit_operation>();
                   has_deposit = true;
                   break;
               default:
                   break;
           }
       }
       FC_ASSERT( has_deposit );

       /* Only support standard withdraw by signature condition with memo */
       FC_ASSERT( withdraw_condition_types( deposit_op.condition.type ) == withdraw_signature_type );
       const withdraw_with_signature withdraw_condition = deposit_op.condition.as<withdraw_with_signature>();
       FC_ASSERT( withdraw_condition.memo.valid() );

       omemo_status status;
       const map<private_key_type, string> account_keys = my->_wallet_db.get_account_private_keys( my->_wallet_password );
       for( const auto& key_item : account_keys )
       {
           const private_key_type& key = key_item.first;
           const string& account_name = key_item.second;

           status = withdraw_condition.decrypt_memo_data( key );
           if( status.valid() )
           {
               my->_wallet_db.cache_memo( *status, key, my->_wallet_password );

               mutable_variant_object info;
               info[ "from" ] = variant();
               info[ "to" ] = account_name;
               info[ "amount" ] = asset( deposit_op.amount, deposit_op.condition.asset_id );
               info[ "memo" ] = variant();

               if( status->has_valid_signature )
               {
                   const address from_address( status->from );
                   const oaccount_record chain_account_record = my->_blockchain->get_account_record( from_address );
                   const owallet_account_record local_account_record = my->_wallet_db.lookup_account( from_address );

                   if( chain_account_record.valid() )
                       info[ "from" ] = chain_account_record->name;
                   else if( local_account_record.valid() )
                       info[ "from" ] = local_account_record->name;
               }

               const string memo = status->get_message();
               if( !memo.empty() )
                   info[ "memo" ] = memo;

               return variant_object( info );
           }
       }

       return optional<variant_object>();
   } FC_CAPTURE_AND_RETHROW() }

   wallet_transaction_record wallet::withdraw_delegate_pay(
           const string& delegate_name,
           double real_amount_to_withdraw,
           const string& withdraw_to_account_name,
           bool sign )
   { try {
       FC_ASSERT( is_open() );
       FC_ASSERT( is_unlocked() );
       FC_ASSERT( my->is_receive_account( delegate_name ) );

       auto asset_rec = my->_blockchain->get_asset_record( asset_id_type(0) );
       share_type amount_to_withdraw((share_type)(real_amount_to_withdraw * asset_rec->precision));

       auto delegate_account_record = my->_blockchain->get_account_record( delegate_name );
       FC_ASSERT( delegate_account_record.valid() );
       FC_ASSERT( delegate_account_record->is_delegate() );

       auto required_fees = get_transaction_fee();
       FC_ASSERT( delegate_account_record->delegate_info->pay_balance >= (amount_to_withdraw + required_fees.amount), "",
                  ("delegate_account_record",delegate_account_record));

       signed_transaction trx;
       unordered_set<address> required_signatures;

       trx.expiration = blockchain::now() + get_transaction_expiration();

       owallet_key_record delegate_key = my->_wallet_db.lookup_key( delegate_account_record->active_key() );
       FC_ASSERT( delegate_key && delegate_key->has_private_key() );
       const auto delegate_private_key = delegate_key->decrypt_private_key( my->_wallet_password );
       const auto delegate_public_key = delegate_private_key.get_public_key();
       required_signatures.insert( delegate_public_key );

       const wallet_account_record receiver_account = get_account( withdraw_to_account_name );
       const string memo_message = "withdraw pay";

       trx.withdraw_pay( delegate_account_record->id, amount_to_withdraw + required_fees.amount );
       trx.deposit_to_account( receiver_account.active_key(),
                               asset( amount_to_withdraw, 0 ),
                               delegate_private_key,
                               memo_message,
                               delegate_public_key,
                               my->get_new_private_key( delegate_name ),
                               from_memo,
                               !receiver_account.is_public_account()
                               );

       auto entry = ledger_entry();
       entry.from_account = delegate_public_key;
       entry.to_account = receiver_account.active_key(),
       entry.amount = asset( amount_to_withdraw );
       entry.memo = memo_message;

       auto record = wallet_transaction_record();
       record.ledger_entries.push_back( entry );
       record.fee = required_fees;

       if( sign )
           my->sign_transaction( trx, required_signatures );

       record.trx = trx;
       return record;
   } FC_CAPTURE_AND_RETHROW( (delegate_name)(real_amount_to_withdraw) ) }

   wallet_transaction_record wallet::burn_asset(
           double real_amount_to_transfer,
           const string& amount_to_transfer_symbol,
           const string& paying_account_name,
           const string& for_or_against,
           const string& to_account_name,
           const string& public_message,
           bool anonymous,
           bool sign
           )
   {
      FC_ASSERT( is_open() );
      FC_ASSERT( is_unlocked() );
      FC_ASSERT( my->_blockchain->is_valid_symbol( amount_to_transfer_symbol ) );
      FC_ASSERT( my->is_receive_account( paying_account_name ) );

      const auto asset_rec = my->_blockchain->get_asset_record( amount_to_transfer_symbol );
      FC_ASSERT( asset_rec.valid() );
      const auto asset_id = asset_rec->id;

      const int64_t precision = asset_rec->precision ? asset_rec->precision : 1;
      share_type amount_to_transfer = real_amount_to_transfer * precision;
      asset asset_to_transfer( amount_to_transfer, asset_id );

      private_key_type sender_private_key  = get_active_private_key( paying_account_name );
      public_key_type  sender_public_key   = sender_private_key.get_public_key();
      address          sender_account_address( sender_private_key.get_public_key() );

      signed_transaction     trx;
      unordered_set<address> required_signatures;

      trx.expiration = blockchain::now() + get_transaction_expiration();

      const auto required_fees = get_transaction_fee( asset_to_transfer.asset_id );
      if( required_fees.asset_id == asset_to_transfer.asset_id )
      {
         my->withdraw_to_transaction( required_fees + asset_to_transfer,
                                      paying_account_name,
                                      trx,
                                      required_signatures );
      }
      else
      {
         my->withdraw_to_transaction( asset_to_transfer,
                                      paying_account_name,
                                      trx,
                                      required_signatures );

         my->withdraw_to_transaction( required_fees,
                                      paying_account_name,
                                      trx,
                                      required_signatures );
      }

      const auto to_account_rec = my->_blockchain->get_account_record( to_account_name );
      optional<signature_type> message_sig;
      if( !anonymous )
      {
         fc::sha256 digest;

         if( public_message.size() )
            digest = fc::sha256::hash( public_message.c_str(), public_message.size() );

         message_sig = sender_private_key.sign_compact( digest );
      }

      FC_ASSERT( to_account_rec.valid() );

      if( to_account_rec->is_retracted() )
          FC_CAPTURE_AND_THROW( account_retracted, (to_account_rec) );

      if( for_or_against == "against" )
         trx.burn( asset_to_transfer, -to_account_rec->id, public_message, message_sig );
      else
      {
         FC_ASSERT( for_or_against == "for" );
         trx.burn( asset_to_transfer, to_account_rec->id, public_message, message_sig );
      }

      auto entry = ledger_entry();
      entry.from_account = sender_public_key;
      entry.amount = asset_to_transfer;
      entry.memo = "burn";
      if( !public_message.empty() )
          entry.memo += ": " + public_message;

      auto record = wallet_transaction_record();
      record.ledger_entries.push_back( entry );
      record.fee = required_fees;
      record.extra_addresses.push_back( to_account_rec->active_key() );

      if( sign )
          my->sign_transaction( trx, required_signatures );

      record.trx = trx;
      return record;
   }

   public_key_type wallet::get_new_public_key( const string& account_name )
   {
       return my->get_new_public_key( account_name );
   }

   address wallet::create_new_address( const string& account_name, const string& label )
   {
       FC_ASSERT( is_open() );
       FC_ASSERT( is_unlocked() );
       FC_ASSERT( my->is_receive_account( account_name ) );
       auto addr = my->get_new_address( account_name, label );
       return addr;
   }

   void wallet::set_address_label( const address& addr, const string& label )
   {
       FC_ASSERT(false, "This doesn't do anything right now.");
       FC_ASSERT( is_open() );
       FC_ASSERT( is_unlocked() );
       auto okey = my->_wallet_db.lookup_key( addr );
       FC_ASSERT( okey.valid(), "No such address." );
       //FC_ASSERT( okey->btc_data.valid(), "Trying to set a label for a TITAN address." );
       //okey->btc_data->label = label;
       my->_wallet_db.store_key( *okey );
   }

   string wallet::get_address_label( const address& addr )
   {
       FC_ASSERT(false, "This doesn't do anything right now.");
       FC_ASSERT( is_open() );
       FC_ASSERT( is_unlocked() );
       auto okey = my->_wallet_db.lookup_key( addr );
       //FC_ASSERT( okey.valid(), "No such address." );
       //FC_ASSERT( okey->btc_data.valid(), "This address has no label (it is a TITAN address)!" );
       //return okey->btc_data->label;
       return "";
   }

   void wallet::set_address_group_label( const address& addr, const string& group_label )
   {
       FC_ASSERT(false, "This doesn't do anything right now.");
       FC_ASSERT( is_open() );
       FC_ASSERT( is_unlocked() );
       auto okey = my->_wallet_db.lookup_key( addr );
       FC_ASSERT( okey.valid(), "No such address." );
       //FC_ASSERT( okey->btc_data.valid(), "Trying to set a group label for a TITAN address" );
       //okey->btc_data->group_label = group_label;
       my->_wallet_db.store_key( *okey );
   }

   string wallet::get_address_group_label( const address& addr )
   {
       FC_ASSERT(false, "This doesn't do anything right now.");
       FC_ASSERT( is_open() );
       FC_ASSERT( is_unlocked() );
       auto okey = my->_wallet_db.lookup_key( addr );
       FC_ASSERT( okey.valid(), "No such address." );
       //FC_ASSERT( okey->btc_data.valid(), "This address has no group label (it is a TITAN address)!" );
       return ""; //return okey->btc_data->group_label;
   }

   vector<address> wallet::get_addresses_for_group_label( const string& group_label )
   {
       FC_ASSERT(false, "This doesn't do anything right now.");
       vector<address> addrs;
       for( auto item : my->_wallet_db.get_keys() )
       {
           auto key = item.second;
           //if( key.btc_data.valid() && key.btc_data->group_label == group_label )
               addrs.push_back( item.first );
       }
       return addrs;
   }

   wallet_transaction_record wallet::transfer_asset_to_address(
           double real_amount_to_transfer,
           const string& amount_to_transfer_symbol,
           const string& from_account_name,
           const address& to_address,
           const string& memo_message,
           vote_strategy strategy,
           bool sign)
   { try {
      FC_ASSERT( is_open() );
      FC_ASSERT( is_unlocked() );
      FC_ASSERT( my->_blockchain->is_valid_symbol( amount_to_transfer_symbol ) );
      FC_ASSERT( my->is_receive_account( from_account_name ) );

      const auto asset_rec = my->_blockchain->get_asset_record( amount_to_transfer_symbol );
      FC_ASSERT( asset_rec.valid() );
      const auto asset_id = asset_rec->id;

      const int64_t precision = asset_rec->precision ? asset_rec->precision : 1;
      share_type amount_to_transfer = real_amount_to_transfer * precision;
      asset asset_to_transfer( amount_to_transfer, asset_id );

      private_key_type sender_private_key  = get_active_private_key( from_account_name );
      public_key_type  sender_public_key   = sender_private_key.get_public_key();
      address          sender_account_address( sender_private_key.get_public_key() );

      signed_transaction     trx;
      unordered_set<address> required_signatures;

      const auto required_fees = get_transaction_fee( asset_to_transfer.asset_id );
      if( required_fees.asset_id == asset_to_transfer.asset_id )
      {
         my->withdraw_to_transaction( required_fees + asset_to_transfer,
                                      from_account_name,
                                      trx,
                                      required_signatures );
      }
      else
      {
         my->withdraw_to_transaction( asset_to_transfer,
                                      from_account_name,
                                      trx,
                                      required_signatures );

         my->withdraw_to_transaction( required_fees,
                                      from_account_name,
                                      trx,
                                      required_signatures );
      }

      trx.deposit( to_address, asset_to_transfer );
      trx.expiration = blockchain::now() + get_transaction_expiration();
      my->set_delegate_slate( trx, strategy );

      if( sign )
          my->sign_transaction( trx, required_signatures );

      auto entry = ledger_entry();
      entry.from_account = sender_public_key;
      entry.amount = asset_to_transfer;
      if( memo_message != "" )
          entry.memo = memo_message;
      else
          entry.memo = "To: " + string(to_address).substr(0, 8) + "...";

      auto record = wallet_transaction_record();
      record.ledger_entries.push_back( entry );
      record.fee = required_fees;
      record.extra_addresses.push_back( to_address );
      record.trx = trx;

      if( sign ) my->sign_transaction( trx, required_signatures );

      return record;
   } FC_CAPTURE_AND_RETHROW( (real_amount_to_transfer)(amount_to_transfer_symbol)(from_account_name)(to_address)(memo_message) ) }


   wallet_transaction_record wallet::transfer_asset_to_many_address(
           const string& amount_to_transfer_symbol,
           const string& from_account_name,
           const std::unordered_map<address, double>& to_address_amounts,
           const string& memo_message,
           bool sign )
   { try {
         FC_ASSERT( is_open() );
         FC_ASSERT( is_unlocked() );
         FC_ASSERT( my->_blockchain->is_valid_symbol( amount_to_transfer_symbol ) );
         FC_ASSERT( my->is_receive_account( from_account_name ) );
         FC_ASSERT( to_address_amounts.size() > 0 );

         auto asset_rec = my->_blockchain->get_asset_record( amount_to_transfer_symbol );
         FC_ASSERT( asset_rec.valid() );
         auto asset_id = asset_rec->id;

         private_key_type sender_private_key  = get_active_private_key( from_account_name );
         public_key_type  sender_public_key   = sender_private_key.get_public_key();
         address          sender_account_address( sender_private_key.get_public_key() );

         signed_transaction     trx;
         unordered_set<address> required_signatures;

         trx.expiration = blockchain::now() + get_transaction_expiration();

         asset total_asset_to_transfer( 0, asset_id );
         auto required_fees = get_transaction_fee();

         vector<address> to_addresses;
         for( const auto& address_amount : to_address_amounts )
         {
            auto real_amount_to_transfer = address_amount.second;
            share_type amount_to_transfer((share_type)(real_amount_to_transfer * asset_rec->precision));
            asset asset_to_transfer( amount_to_transfer, asset_id );

            my->withdraw_to_transaction( asset_to_transfer,
                                         from_account_name,
                                         trx,
                                         required_signatures );

            total_asset_to_transfer += asset_to_transfer;

            trx.deposit( address_amount.first, asset_to_transfer );

            to_addresses.push_back( address_amount.first );
         }

         my->withdraw_to_transaction( required_fees,
                                      from_account_name,
                                      trx,
                                      required_signatures );

         auto entry = ledger_entry();
         entry.from_account = sender_public_key;
         entry.amount = total_asset_to_transfer;
         if( memo_message != "" )
             entry.memo = memo_message;
         else
             entry.memo = "Transfer to many addresses";

         auto record = wallet_transaction_record();
         record.ledger_entries.push_back( entry );
         record.fee = required_fees;
         record.extra_addresses = to_addresses;

         if( sign )
             my->sign_transaction( trx, required_signatures );

         record.trx = trx;
         return record;
   } FC_CAPTURE_AND_RETHROW( (amount_to_transfer_symbol)(from_account_name)(to_address_amounts)(memo_message) ) }

   wallet_transaction_record wallet::register_account(
           const string& account_to_register,
           const variant& public_data,
           uint8_t delegate_pay_rate,
           const string& pay_with_account_name,
           account_type new_account_type,
           bool sign )
   { try {
      if( !my->_blockchain->is_valid_account_name( account_to_register ) )
          FC_THROW_EXCEPTION( invalid_name, "Invalid account name!", ("account_to_register",account_to_register) );

      FC_ASSERT( is_open() );
      FC_ASSERT( is_unlocked() );

      const auto registered_account = my->_blockchain->get_account_record( account_to_register );
      if( registered_account.valid() )
          FC_THROW_EXCEPTION( duplicate_account_name, "This account name has already been registered!" );

      const auto payer_public_key = get_owner_public_key( pay_with_account_name );
      address from_account_address( payer_public_key );

      const auto account_public_key = get_owner_public_key( account_to_register );

      signed_transaction     trx;
      unordered_set<address> required_signatures;

      trx.expiration = blockchain::now() + get_transaction_expiration();

      optional<account_meta_info> meta_info;
      if( new_account_type == public_account )
         meta_info = account_meta_info( public_account );

      // TODO: This is a hack to register with different owner and active keys until the API is fixed
      try
      {
          const wallet_account_record local_account = get_account( account_to_register );
          trx.register_account( account_to_register,
                                public_data,
                                local_account.owner_key,
                                local_account.active_key(),
                                delegate_pay_rate <= 100 ? delegate_pay_rate : -1,
                                meta_info );
      }
      catch( ... )
      {
          trx.register_account( account_to_register,
                                public_data,
                                account_public_key, // master
                                account_public_key, // active
                                delegate_pay_rate <= 100 ? delegate_pay_rate : -1,
                                meta_info );
      }

      // Verify a parent key is available if required
      optional<string> parent_name = my->_blockchain->get_parent_account_name( account_to_register );
      if( parent_name.valid() )
      {
          bool have_parent_key = false;
          for( ; parent_name.valid(); parent_name = my->_blockchain->get_parent_account_name( *parent_name ) )
          {
              try
              {
                  const wallet_account_record parent_record = get_account( *parent_name );
                  if( parent_record.is_retracted() )
                      continue;

                  if( my->_wallet_db.has_private_key( parent_record.active_address() ) )
                  {
                      required_signatures.insert( parent_record.active_address() );
                      have_parent_key = true;
                      break;
                  }

                  if( my->_wallet_db.has_private_key( parent_record.owner_address() ) )
                  {
                      required_signatures.insert( parent_record.owner_address() );
                      have_parent_key = true;
                      break;
                  }
              }
              catch( ... )
              {
              }
          }
          if( !have_parent_key )
          {
              FC_THROW_EXCEPTION( unauthorized_child_account, "Parent account must authorize registration of this child account!",
                                  ("child_account",account_to_register) );
          }
      }

      auto required_fees = get_transaction_fee();

      bool as_delegate = false;
      if( delegate_pay_rate <= 100  )
      {
        required_fees += asset(my->_blockchain->get_delegate_registration_fee(delegate_pay_rate),0);
        as_delegate = true;
      }

      my->withdraw_to_transaction( required_fees,
                                   pay_with_account_name,
                                   trx,
                                   required_signatures );

      auto entry = ledger_entry();
      entry.from_account = payer_public_key;
      entry.to_account = account_public_key;
      entry.memo = "register " + account_to_register + (as_delegate ? " as a delegate" : "");

      auto record = wallet_transaction_record();
      record.ledger_entries.push_back( entry );
      record.fee = required_fees;

      if( sign )
          my->sign_transaction( trx, required_signatures );

      record.trx = trx;
      return record;
   } FC_CAPTURE_AND_RETHROW( (account_to_register)(public_data)(pay_with_account_name)(delegate_pay_rate) ) }

   wallet_transaction_record wallet::create_asset(
           const string& symbol,
           const string& asset_name,
           const string& description,
           const variant& data,
           const string& issuer_account_name,
           double max_share_supply,
           uint64_t precision,
           bool is_market_issued,
           bool sign )
   { try {
      FC_ASSERT( blockchain::is_power_of_ten( precision ) );
      FC_ASSERT( is_open() );
      FC_ASSERT( is_unlocked() );
      FC_ASSERT( my->_blockchain->is_valid_symbol_name( symbol ) ); // valid length and characters
      FC_ASSERT( ! my->_blockchain->is_valid_symbol( symbol ) ); // not yet registered

      signed_transaction     trx;
      unordered_set<address> required_signatures;

      trx.expiration = blockchain::now() + get_transaction_expiration();

      auto required_fees = get_transaction_fee();

      required_fees += asset(my->_blockchain->get_asset_registration_fee(symbol.size()),0);

      if( !my->_blockchain->is_valid_account_name( issuer_account_name ) )
          FC_THROW_EXCEPTION( invalid_name, "Invalid account name!", ("issuer_account_name",issuer_account_name) );
      auto from_account_address = get_owner_public_key( issuer_account_name );
      auto oname_rec = my->_blockchain->get_account_record( issuer_account_name );
      if( !oname_rec.valid() )
          FC_THROW_EXCEPTION( account_not_registered, "Assets can only be created by registered accounts", ("issuer_account_name",issuer_account_name) );

      my->withdraw_to_transaction( required_fees,
                                   issuer_account_name,
                                   trx,
                                   required_signatures );

      required_signatures.insert(oname_rec->active_key());

      //check this way to avoid overflow
      FC_ASSERT(BTS_BLOCKCHAIN_MAX_SHARES / precision > max_share_supply);
      share_type max_share_supply_in_internal_units = max_share_supply * precision;
      if( NOT is_market_issued )
      {
         trx.create_asset( symbol, asset_name,
                           description, data,
                           oname_rec->id, max_share_supply_in_internal_units, precision );
      }
      else
      {
         trx.create_asset( symbol, asset_name,
                           description, data,
                           asset_record::market_issuer_id, max_share_supply_in_internal_units, precision );
      }

      auto entry = ledger_entry();
      entry.from_account = from_account_address;
      entry.to_account = from_account_address;
      entry.memo = "create " + symbol + " (" + asset_name + ")";

      auto record = wallet_transaction_record();
      record.ledger_entries.push_back( entry );
      record.fee = required_fees;

      if( sign )
          my->sign_transaction( trx, required_signatures );

      record.trx = trx;
      return record;
   } FC_CAPTURE_AND_RETHROW( (symbol)(asset_name)(description)(issuer_account_name) ) }

   wallet_transaction_record wallet::update_asset(
           const string& symbol,
           const optional<string>& name,
           const optional<string>& description,
           const optional<variant>& public_data,
           const optional<double>& maximum_share_supply,
           const optional<uint64_t>& precision,
           const share_type issuer_fee,
           double market_fee,
           uint32_t flags,
           uint32_t issuer_perms,
           const string& issuer_account_name,
           uint32_t required_sigs,
           const vector<address>& authority,
           bool sign
           )
   { try {
      if( NOT is_open()     ) FC_CAPTURE_AND_THROW( wallet_closed );
      if( NOT is_unlocked() ) FC_CAPTURE_AND_THROW( wallet_locked );

      optional<account_id_type> issuer_account_id;
      if( issuer_account_name != "" )
      {
         auto issuer_account = my->_blockchain->get_account_record( issuer_account_name );
         FC_ASSERT( issuer_account.valid() );
         issuer_account_id = issuer_account->id;
	  }

      transaction_builder_ptr builder = create_transaction_builder();
      builder->update_asset( symbol, name, description, public_data, maximum_share_supply, precision,
                             issuer_fee, market_fee, flags, issuer_perms, issuer_account_id, required_sigs, authority );
      builder->finalize();

      if( sign )
         return builder->sign();
      return builder->transaction_record;
   } FC_CAPTURE_AND_RETHROW( (symbol)(name)(description)(public_data)(precision)(issuer_fee)(restricted)(retractable)(required_sigs)(authority)(sign) ) }

   wallet_transaction_record wallet::issue_asset(
           double amount_to_issue,
           const string& symbol,
           const string& to_account_name,
           const string& memo_message,
           bool sign )
   { try {
      FC_ASSERT( is_open() );
      FC_ASSERT( is_unlocked() );
      FC_ASSERT( my->_blockchain->is_valid_symbol( symbol ) );

      signed_transaction         trx;
      unordered_set<address>     required_signatures;
      trx.expiration = blockchain::now() + get_transaction_expiration();

      auto required_fees = get_transaction_fee();

      auto asset_record = my->_blockchain->get_asset_record( symbol );
      FC_ASSERT(asset_record.valid(), "no such asset record");
      auto issuer_account = my->_blockchain->get_account_record( asset_record->issuer_account_id );
      FC_ASSERT(issuer_account, "uh oh! no account for valid asset");
      auto authority = asset_record->authority;

      asset shares_to_issue( amount_to_issue * asset_record->precision, asset_record->id );
      my->withdraw_to_transaction( required_fees,
                                   issuer_account->name,
                                   trx,
                                   required_signatures );

      trx.issue( shares_to_issue );
      for( auto owner : authority.owners )
          required_signatures.insert( owner );

      owallet_account_record issuer = my->_wallet_db.lookup_account( asset_record->issuer_account_id );
      FC_ASSERT( issuer.valid() );
      owallet_key_record issuer_key = my->_wallet_db.lookup_key( issuer->active_address() );
      FC_ASSERT( issuer_key && issuer_key->has_private_key() );
      auto sender_private_key = issuer_key->decrypt_private_key( my->_wallet_password );

      const wallet_account_record receiver_account = get_account( to_account_name );

      trx.deposit_to_account( receiver_account.active_key(),
                              shares_to_issue,
                              sender_private_key,
                              memo_message,
                              issuer->active_key(),
                              my->get_new_private_key( issuer_account->name ),
                              from_memo,
                              !receiver_account.is_public_account()
                              );

      auto entry = ledger_entry();
      entry.from_account = issuer->active_key();
      entry.to_account = receiver_account.active_key();
      entry.amount = shares_to_issue;
      entry.memo = "issue " + my->_blockchain->to_pretty_asset( shares_to_issue );

      auto record = wallet_transaction_record();
      record.ledger_entries.push_back( entry );
      record.fee = required_fees;

      if( sign )
          my->sign_transaction( trx, required_signatures );

      record.trx = trx;
      return record;
   } FC_CAPTURE_AND_RETHROW() }


   wallet_transaction_record wallet::issue_asset_to_addresses(
           const string& symbol,
           const map<string, share_type>& addresses )
   { try {
      FC_ASSERT( is_open() );
      FC_ASSERT( is_unlocked() );
      FC_ASSERT( my->_blockchain->is_valid_symbol( symbol ) );

      signed_transaction         trx;
      unordered_set<address>     required_signatures;

      trx.expiration = blockchain::now() + get_transaction_expiration();

      auto required_fees = get_transaction_fee();

      auto asset_record = my->_blockchain->get_asset_record( symbol );
      FC_ASSERT(asset_record.valid(), "no such asset record");
      auto issuer_account = my->_blockchain->get_account_record( asset_record->issuer_account_id );
      FC_ASSERT(issuer_account, "uh oh! no account for valid asset");
      auto authority = asset_record->authority;

      asset shares_to_issue( 0, asset_record->id );

      for( auto pair : addresses )
      {
          auto addr = address( pair.first );
          auto amount = asset( pair.second, asset_record->id );
          trx.deposit( addr, amount );
          shares_to_issue += amount;
      }
      my->withdraw_to_transaction( required_fees,
                                   issuer_account->name,
                                   trx,
                                   required_signatures );
      trx.issue( shares_to_issue );
      for( auto owner : authority.owners )
          required_signatures.insert( owner );
//      required_signatures.insert( issuer_account->active_key() );

      owallet_account_record issuer = my->_wallet_db.lookup_account( asset_record->issuer_account_id );
      FC_ASSERT( issuer.valid() );
      owallet_key_record  issuer_key = my->_wallet_db.lookup_key( issuer->owner_address() );
      FC_ASSERT( issuer_key && issuer_key->has_private_key() );

      auto entry = ledger_entry();
      entry.from_account = issuer->active_key();
      entry.amount = shares_to_issue;
      entry.memo = "issue to many addresses";

      auto record = wallet_transaction_record();
      record.ledger_entries.push_back( entry );
      record.fee = required_fees;

  //    if( sign )
      my->sign_transaction( trx, required_signatures );

      record.trx = trx;
      return record;
   } FC_CAPTURE_AND_RETHROW() }




   void wallet::update_account_private_data( const string& account_to_update,
                                             const variant& private_data )
   {
      get_account( account_to_update ); /* Just to check input */
      auto oacct = my->_wallet_db.lookup_account( account_to_update );
      FC_ASSERT( oacct.valid() );
      oacct->private_data = private_data;
      my->_wallet_db.store_account( *oacct );
   }

   wallet_transaction_record wallet::update_registered_account(
           const string& account_to_update,
           const string& pay_from_account,
           optional<variant> public_data,
           uint8_t delegate_pay_rate,
           bool sign )
   { try {
      FC_ASSERT( is_unlocked() );

      auto account = get_account(account_to_update);
      owallet_account_record payer;
         if( !pay_from_account.empty() ) payer = get_account(pay_from_account);

      optional<uint8_t> pay;
      if( delegate_pay_rate <= 100 )
          pay = delegate_pay_rate;

      transaction_builder_ptr builder = create_transaction_builder();
      builder->update_account_registration(account, public_data, optional<public_key_type>(), pay, payer).
               finalize();
      if( sign )
         return builder->sign();
      return builder->transaction_record;
   } FC_CAPTURE_AND_RETHROW( (account_to_update)(pay_from_account)(public_data)(sign) ) }

   wallet_transaction_record wallet::update_active_key(
           const std::string& account_to_update,
           const std::string& pay_from_account,
           const std::string& new_active_key,
           bool sign )
   { try {
      FC_ASSERT( is_unlocked() );

      auto account = get_account(account_to_update);
      owallet_account_record payer;
      if( !pay_from_account.empty() ) payer = get_account(pay_from_account);

      public_key_type new_public_key;
      if( new_active_key.empty() )
      {
        new_public_key = my->get_new_public_key( account_to_update );
      }
      else
      {
        const optional<private_key_type> new_private_key = utilities::wif_to_key(new_active_key);
        FC_ASSERT(new_private_key.valid(), "Unable to parse new active key.");
        new_public_key = import_private_key( *new_private_key, account_to_update, false );
      }

      transaction_builder_ptr builder = create_transaction_builder();
      builder->update_account_registration(account, optional<variant>(), new_public_key, optional<share_type>(), payer).
               finalize();
      if( sign )
      {
          my->_dirty_accounts = true;
          return builder->sign();
      }
      return builder->transaction_record;
   } FC_CAPTURE_AND_RETHROW( (account_to_update)(pay_from_account)(sign) ) }

   wallet_transaction_record wallet::retract_account(
           const std::string& account_to_retract,
           const std::string& pay_from_account,
           bool sign )
   { try {
      FC_ASSERT( is_unlocked() );

      auto account = get_account(account_to_retract);
      owallet_account_record payer;
      if( !pay_from_account.empty() ) payer = get_account(pay_from_account);

      fc::ecc::public_key empty_pk;
      public_key_type new_public_key(empty_pk);

      transaction_builder_ptr builder = create_transaction_builder();
      builder->update_account_registration(account, optional<variant>(), new_public_key, optional<share_type>(), payer).
               finalize();
      if( sign )
         return builder->sign();
      return builder->transaction_record;
   } FC_CAPTURE_AND_RETHROW( (account_to_retract)(pay_from_account)(sign) ) }

   wallet_transaction_record wallet::cancel_market_orders(
           const vector<order_id_type>& order_ids,
           bool sign )
   { try {
      if( NOT is_open()     ) FC_CAPTURE_AND_THROW( wallet_closed );
      if( NOT is_unlocked() ) FC_CAPTURE_AND_THROW( wallet_locked );

      transaction_builder_ptr builder = create_transaction_builder();

      for( const auto& order_id : order_ids )
         builder->cancel_market_order(order_id);
      builder->finalize();

      if( sign )
         return builder->sign();
      return builder->transaction_record;
   } FC_CAPTURE_AND_RETHROW( (order_ids) ) }

   wallet_transaction_record wallet::batch_market_update(const vector<order_id_type>& cancel_order_ids,
                                                         const vector<std::pair<order_type_enum,vector<string>>>&
                                                                     new_orders,
                                                         bool sign)
   { try {
      if( !is_open()     ) FC_CAPTURE_AND_THROW( wallet_closed );
      if( !is_unlocked() ) FC_CAPTURE_AND_THROW( wallet_locked );

      transaction_builder_ptr builder = create_transaction_builder();

      for( const auto& id : cancel_order_ids )
         builder->cancel_market_order(id);

      for( const auto& order_description : new_orders)
      {
         auto& args = order_description.second;

         switch( order_description.first )
         {
         case cover_order:
            FC_ASSERT(args.size() > 3, "Incorrect number of arguments.");
            //args: from_account_name, quantity, symbol, ID
            builder->submit_cover(get_account(args[0]),
                                 my->_blockchain->to_ugly_asset(args[1], args[2]),
                                 fc::ripemd160(args[3]));
            break;
         case bid_order:
         case ask_order:
         case relative_bid_order:
         case relative_ask_order:
         case short_order:
            //args: account_name, quantity, base_symbol, price, quote_symbol[, price_limit (for shorts)]
            FC_ASSERT(args.size() > 4, "Incorrect number of arguments.");
            my->apply_order_to_builder(order_description.first, builder,
                                       args[0], args[1], args[3], args[2], args[4],
                                       true,
                                       //For shorts:
                                       args.size() > 5? args[5] : string()
                                       );
            break;
         default:
            FC_THROW_EXCEPTION( invalid_operation, "Unknown operation type ${op}", ("op", order_description.first) );
         }
      }

      builder->finalize();

      if( sign )
         return builder->sign();
      return builder->transaction_record;
   } FC_CAPTURE_AND_RETHROW( (cancel_order_ids)(new_orders) ) }

   wallet_transaction_record wallet::submit_bid(
           const string& from_account_name,
           const string& real_quantity,
           const string& quantity_symbol,
           const string& quote_price,
           const string& quote_symbol,
           bool sign )
   { try {
      if( NOT is_open()     ) FC_CAPTURE_AND_THROW( wallet_closed );
      if( NOT is_unlocked() ) FC_CAPTURE_AND_THROW( wallet_locked );

      transaction_builder_ptr builder = create_transaction_builder();
      my->apply_order_to_builder(bid_order,
                                 builder,
                                 from_account_name,
                                 real_quantity,
                                 quote_price,
                                 quantity_symbol,
                                 quote_symbol,
                                 true
                                 );
      builder->finalize();

      if( sign )
         return builder->sign();
      return builder->transaction_record;
   } FC_CAPTURE_AND_RETHROW( (from_account_name)
                             (real_quantity)(quantity_symbol)
                             (quote_price)(quote_symbol)(sign) ) }


   wallet_transaction_record wallet::sell(
           const string& from_account,
           const string& sell_quantity,
           const string& sell_quantity_symbol,
           const string& price_limit,
           const string& price_symbol,
           const string& relative_percent,
           bool allow_stupid,
           bool sign
   )
   {
      try
      {
         if( NOT is_open()     )
             FC_CAPTURE_AND_THROW( wallet_closed );
         if( NOT is_unlocked() )
             FC_CAPTURE_AND_THROW( wallet_locked );

         transaction_builder_ptr builder = create_transaction_builder();
         my->apply_sell_order_to_builder(
                                    builder,
                                    from_account,
                                    sell_quantity,
                                    sell_quantity_symbol,
                                    price_limit,
                                    price_symbol,
                                    relative_percent,
                                    allow_stupid
                                    );
         builder->finalize();

         if( sign )
            return builder->sign();
         return builder->transaction_record;
      } FC_CAPTURE_AND_RETHROW( (from_account)
                                (sell_quantity)(sell_quantity_symbol)
                                (price_limit)(price_symbol)
                                (relative_percent)(sign) )
   }

   wallet_transaction_record wallet::submit_relative_ask(
           const string& from_account_name,
           const string& real_quantity,
           const string& quantity_symbol,
           const string& relative_quote_price,
           const string& quote_symbol,
           const string& limit_price,
           bool sign )
   { try {
      if( NOT is_open()     ) FC_CAPTURE_AND_THROW( wallet_closed );
      if( NOT is_unlocked() ) FC_CAPTURE_AND_THROW( wallet_locked );

      oasset_record quantity_arec = my->_blockchain->get_asset_record( quantity_symbol );
      oasset_record quote_arec = my->_blockchain->get_asset_record( quote_symbol );

      // TODO: Add symbol to error messages, use exception ID instead of assert
	  FC_ASSERT( quantity_arec.valid(), "could not find record for asset being sold" );
	  FC_ASSERT( quote_arec.valid(), "could not find record for asset being bought" );

	  const asset_id_type quantity_id = quantity_arec->id;
	  const asset_id_type quote_id = quote_arec->id;

      if( quantity_id == quote_id )
          FC_CAPTURE_AND_THROW( invalid_market, (quantity_symbol)(quote_symbol) );

      const bool is_bid = (quantity_id > quote_id);
      if( is_bid )
          FC_CAPTURE_AND_THROW( invalid_market, (quantity_symbol)(quote_symbol) );

      transaction_builder_ptr builder = create_transaction_builder();

      // go through sell order, but yell if asset ID mismatch
      my->apply_sell_order_to_builder(
          builder,
          from_account_name,
          real_quantity,
          quantity_symbol,
          limit_price,
          quote_symbol,
          relative_quote_price,
          true
          );

      builder->finalize();

      if( sign )
         return builder->sign();
      return builder->transaction_record;
   } FC_CAPTURE_AND_RETHROW( (from_account_name)
                             (real_quantity)(quantity_symbol)
                             (relative_quote_price)(quote_symbol)(limit_price)(sign) ) }


   wallet_transaction_record wallet::submit_ask(
           const string& from_account_name,
           const string& real_quantity,
           const string& quantity_symbol,
           const string& quote_price,
           const string& quote_symbol,
           bool sign )
   { try {
      if( NOT is_open()     ) FC_CAPTURE_AND_THROW( wallet_closed );
      if( NOT is_unlocked() ) FC_CAPTURE_AND_THROW( wallet_locked );

      transaction_builder_ptr builder = create_transaction_builder();
      my->apply_order_to_builder(ask_order,
                                 builder,
                                 from_account_name,
                                 real_quantity,
                                 quote_price,
                                 quantity_symbol,
                                 quote_symbol,
                                 true);
      builder->finalize();

      if( sign )
         return builder->sign();
      return builder->transaction_record;

   } FC_CAPTURE_AND_RETHROW( (from_account_name)
                             (real_quantity)(quantity_symbol)
                             (quote_price)(quote_symbol)(sign) ) }

   wallet_transaction_record wallet::submit_short(const string& from_account_name,
                                                  const string& real_quantity_xts,
                                                  const string& collateral_symbol,
                                                  const string& apr,
                                                  const string& quote_symbol,
                                                  const string& price_limit,
                                                  bool sign)
   { try {
      if( NOT is_open()     ) FC_CAPTURE_AND_THROW( wallet_closed );
      if( NOT is_unlocked() ) FC_CAPTURE_AND_THROW( wallet_locked );

      transaction_builder_ptr builder = create_transaction_builder();
      my->apply_order_to_builder(short_order,
                                 builder,
                                 from_account_name,
                                 real_quantity_xts,
                                 apr,
                                 collateral_symbol,
                                 quote_symbol,
                                 false,
                                 price_limit);
      builder->finalize();

      if( sign )
         return builder->sign();
      return builder->transaction_record;
   } FC_CAPTURE_AND_RETHROW( (from_account_name)
                             (real_quantity_xts)(quote_symbol)
                             (apr)(collateral_symbol)
                             (price_limit)(sign) ) }

   wallet_transaction_record wallet::add_collateral(
           const string& from_account_name,
           const order_id_type& cover_id,
           const string& real_quantity_collateral_to_add,
           bool sign )
   { try {
       if (!is_open()) FC_CAPTURE_AND_THROW (wallet_closed);
       if (!is_unlocked()) FC_CAPTURE_AND_THROW (wallet_locked);
       if (!my->is_receive_account(from_account_name)) FC_CAPTURE_AND_THROW (unknown_receive_account);

       const auto order = my->_blockchain->get_market_order( cover_id, cover_order );
       if( !order.valid() )
           FC_THROW_EXCEPTION( unknown_market_order, "Cannot find that cover order!" );

       const oasset_record base_asset = my->_blockchain->get_asset_record( order->market_index.order_price.base_asset_id );
       FC_ASSERT( base_asset.valid() );

       const asset collateral_to_add = my->_blockchain->to_ugly_asset( real_quantity_collateral_to_add,
                                                                       base_asset->symbol );

       if (collateral_to_add.amount <= 0) FC_CAPTURE_AND_THROW (bad_collateral_amount);

       const auto owner_address = order->get_owner();
       const auto owner_key_record = my->_wallet_db.lookup_key( owner_address );
       // TODO: Throw proper exception
       FC_ASSERT( owner_key_record.valid() && owner_key_record->has_private_key() );

       auto     from_account_key = get_owner_public_key( from_account_name );
       address  from_address( from_account_key );

       signed_transaction trx;
       unordered_set<address> required_signatures;

       trx.expiration = blockchain::now() + get_transaction_expiration();
       required_signatures.insert( owner_address );

       trx.add_collateral( collateral_to_add.amount, order->market_index );

       auto required_fees = get_transaction_fee();
       my->withdraw_to_transaction( collateral_to_add + required_fees,
                                    from_account_name,
                                    trx,
                                    required_signatures );

       auto record = wallet_transaction_record();
       record.is_market = true;
       record.fee = required_fees;

       auto entry = ledger_entry();
       entry.from_account = from_account_key;
       entry.to_account = get_private_key( owner_address ).get_public_key();
       entry.amount = collateral_to_add;
       entry.memo = "add collateral to short";
       record.ledger_entries.push_back(entry);

       if( sign )
           my->sign_transaction( trx, required_signatures );

       record.trx = trx;
       return record;
   } FC_CAPTURE_AND_RETHROW((from_account_name)(cover_id)(real_quantity_collateral_to_add)(sign)) }

   wallet_transaction_record wallet::cover_short(
           const string& from_account_name,
           const string& real_quantity_usd,
           const string& quote_symbol,
           const order_id_type& cover_id,
           bool sign )
   { try {
       if( NOT is_open()     ) FC_CAPTURE_AND_THROW( wallet_closed );
       if( NOT is_unlocked() ) FC_CAPTURE_AND_THROW( wallet_locked );

       transaction_builder_ptr builder = create_transaction_builder();
       builder->submit_cover(get_account(from_account_name),
                             my->_blockchain->to_ugly_asset(real_quantity_usd, quote_symbol),
                             cover_id);
       builder->finalize();

       if( sign )
          return builder->sign();
       return builder->transaction_record;
   } FC_CAPTURE_AND_RETHROW( (from_account_name)(real_quantity_usd)(quote_symbol)(cover_id)(sign) ) }

   void wallet::set_transaction_fee( const asset& fee )
   { try {
      FC_ASSERT( is_open() );

      if( fee.amount < 0 || fee.asset_id != 0 )
          FC_THROW_EXCEPTION( invalid_fee, "Invalid transaction fee!", ("fee",fee) );

      my->_wallet_db.set_property( default_transaction_priority_fee, variant( fee ) );
   } FC_CAPTURE_AND_RETHROW( (fee) ) }

   asset wallet::get_transaction_fee( const asset_id_type desired_fee_asset_id )const
   { try {
      FC_ASSERT( is_open() );
      // TODO: support price conversion using price from blockchain

      asset xts_fee( BTS_WALLET_DEFAULT_TRANSACTION_FEE, 0 );
      try
      {
          xts_fee = my->_wallet_db.get_property( default_transaction_priority_fee ).as<asset>();
      }
      catch( ... )
      {
      }

      if( desired_fee_asset_id != 0 )
      {
         const auto asset_rec = my->_blockchain->get_asset_record( desired_fee_asset_id );
         if( asset_rec.valid() && asset_rec->is_market_issued() )
         {
             const auto median_price = my->_blockchain->get_active_feed_price( desired_fee_asset_id );
             if( median_price.valid() )
             {
                // fees paid in something other than XTS are discounted 50%
                xts_fee += xts_fee + xts_fee;
                return xts_fee * *median_price;
             }
         }
      }

      return xts_fee;
   } FC_CAPTURE_AND_RETHROW( (desired_fee_asset_id) ) }

   bool wallet::asset_can_pay_fee(const asset_id_type desired_fee_asset_id) const
   {
      return get_transaction_fee(desired_fee_asset_id).asset_id == desired_fee_asset_id;
   }

   void wallet::set_last_scanned_block_number( uint32_t block_num )
   { try {
       FC_ASSERT( is_open() );
       my->_wallet_db.set_property( last_unlocked_scanned_block_number, fc::variant( block_num ) );
   } FC_CAPTURE_AND_RETHROW() }

   uint32_t wallet::get_last_scanned_block_number()const
   { try {
       FC_ASSERT( is_open() );
       try
       {
           return my->_wallet_db.get_property( last_unlocked_scanned_block_number ).as<uint32_t>();
       }
       catch( ... )
       {
       }
       return my->_blockchain->get_head_block_num();
   } FC_CAPTURE_AND_RETHROW() }

   void wallet::set_transaction_expiration( uint32_t secs )
   { try {
       FC_ASSERT( is_open() );

       if( secs > BTS_BLOCKCHAIN_MAX_TRANSACTION_EXPIRATION_SEC )
          FC_THROW_EXCEPTION( invalid_expiration_time, "Invalid expiration time!", ("secs",secs) );

       my->_wallet_db.set_property( transaction_expiration_sec, fc::variant( secs ) );
   } FC_CAPTURE_AND_RETHROW() }

   uint32_t wallet::get_transaction_expiration()const
   { try {
       FC_ASSERT( is_open() );
       return my->_wallet_db.get_property( transaction_expiration_sec ).as<uint32_t>();
   } FC_CAPTURE_AND_RETHROW() }

   float wallet::get_scan_progress()const
   { try {
       FC_ASSERT( is_open() );
       return my->_scan_progress;
   } FC_CAPTURE_AND_RETHROW() }

   string wallet::get_key_label( const public_key_type& key )const
   { try {
       if( key == public_key_type() )
           return "ANONYMOUS";

       auto account_record = my->_wallet_db.lookup_account( key );
       if( account_record.valid() )
           return account_record->name;

       const auto blockchain_account_record = my->_blockchain->get_account_record( key );
       if( blockchain_account_record.valid() )
          return blockchain_account_record->name;

       const auto key_record = my->_wallet_db.lookup_key( key );
       if( key_record.valid() )
       {
           if( key_record->memo.valid() )
               return *key_record->memo;

           account_record = my->_wallet_db.lookup_account( key_record->account_address );
           if( account_record.valid() )
               return account_record->name;
       }

       return string( key );
   } FC_CAPTURE_AND_RETHROW( (key) ) }

   vector<string> wallet::list() const
   {
       FC_ASSERT(is_enabled(), "Wallet is not enabled in this client!");

       vector<string> wallets;
       if (!fc::is_directory(get_data_directory()))
           return wallets;

       auto path = get_data_directory();
       fc::directory_iterator end_itr; // constructs terminator
       for( fc::directory_iterator itr( path ); itr != end_itr; ++itr)
       {
          if (!itr->stem().string().empty() && fc::is_directory( *itr ))
          {
              wallets.push_back( (*itr).stem().string() );
          }
       }

       std::sort( wallets.begin(), wallets.end() );
       return wallets;
   }

   bool wallet::is_sending_address( const address& addr )const
   { try {
      return !is_receive_address( addr );
   } FC_CAPTURE_AND_RETHROW() }


   bool wallet::is_receive_address( const address& addr )const
   {  try {
      auto key_rec = my->_wallet_db.lookup_key( addr );
      if( key_rec.valid() )
         return key_rec->has_private_key();
      return false;
   } FC_CAPTURE_AND_RETHROW() }

   vector<wallet_account_record> wallet::list_accounts() const
   { try {
      const auto& accs = my->_wallet_db.get_accounts();

      vector<wallet_account_record> accounts;
      accounts.reserve( accs.size() );
      for( const auto& item : accs )
          accounts.push_back( item.second );

      std::sort( accounts.begin(), accounts.end(),
                 [](const wallet_account_record& a, const wallet_account_record& b) -> bool
                 { return a.name.compare( b.name ) < 0; } );

      return accounts;
   } FC_CAPTURE_AND_RETHROW() }

   vector<wallet_account_record> wallet::list_my_accounts() const
   { try {
      const auto& accs = my->_wallet_db.get_accounts();

      vector<wallet_account_record> receive_accounts;
      receive_accounts.reserve( accs.size() );
      for( const auto& item : accs )
         if ( item.second.is_my_account )
            receive_accounts.push_back( item.second );

      std::sort( receive_accounts.begin(), receive_accounts.end(),
                 [](const wallet_account_record& a, const wallet_account_record& b) -> bool
                 { return a.name.compare( b.name ) < 0; } );

      return receive_accounts;
   } FC_CAPTURE_AND_RETHROW() }


   vector<wallet_account_record> wallet::list_favorite_accounts() const
   { try {
      const auto& accs = my->_wallet_db.get_accounts();

      vector<wallet_account_record> receive_accounts;
      receive_accounts.reserve( accs.size() );
      for( const auto& item : accs )
      {
         if( item.second.is_favorite )
         {
            receive_accounts.push_back( item.second );
         }
      }

      std::sort( receive_accounts.begin(), receive_accounts.end(),
                 [](const wallet_account_record& a, const wallet_account_record& b) -> bool
                 { return a.name.compare( b.name ) < 0; } );

      return receive_accounts;
   } FC_CAPTURE_AND_RETHROW() }

   vector<wallet_account_record> wallet::list_unregistered_accounts() const
   { try {
      const auto& accs = my->_wallet_db.get_accounts();

      vector<wallet_account_record> receive_accounts;
      receive_accounts.reserve( accs.size() );
      for( const auto& item : accs )
      {
         if( item.second.id == 0 )
         {
            receive_accounts.push_back( item.second );
         }
      }

      std::sort( receive_accounts.begin(), receive_accounts.end(),
                 [](const wallet_account_record& a, const wallet_account_record& b) -> bool
                 { return a.name.compare( b.name ) < 0; } );

      return receive_accounts;
   } FC_CAPTURE_AND_RETHROW() }

   vector<wallet_transaction_record> wallet::get_pending_transactions()const
   {
       return my->get_pending_transactions();
   }

   map<transaction_id_type, fc::exception> wallet::get_pending_transaction_errors()const
   { try {
       map<transaction_id_type, fc::exception> transaction_errors;
       const auto& transaction_records = get_pending_transactions();
       const auto relay_fee = my->_blockchain->get_relay_fee();
       for( const auto& transaction_record : transaction_records )
       {
           FC_ASSERT( !transaction_record.is_virtual && !transaction_record.is_confirmed );
           const auto error = my->_blockchain->get_transaction_error( transaction_record.trx, relay_fee );
           if( !error.valid() ) continue;
           transaction_errors[ transaction_record.trx.id() ] = *error;
       }
       return transaction_errors;
   } FC_CAPTURE_AND_RETHROW() }

   private_key_type wallet::get_active_private_key( const string& account_name )const
   { try {
      if( !my->_blockchain->is_valid_account_name( account_name ) )
          FC_THROW_EXCEPTION( invalid_name, "Invalid account name!", ("account_name",account_name) );
      FC_ASSERT( is_open() );
      FC_ASSERT( is_unlocked() );
      auto opt_account = my->_wallet_db.lookup_account( account_name );
      FC_ASSERT( opt_account.valid(), "Unable to find account '${name}'",
                ("name",account_name) );

      auto opt_key = my->_wallet_db.lookup_key( opt_account->active_address() );
      FC_ASSERT( opt_key.valid(), "Unable to find key for account '${name}",
                ("name",account_name) );

      FC_ASSERT( opt_key->has_private_key() );
      return opt_key->decrypt_private_key( my->_wallet_password );
   } FC_CAPTURE_AND_RETHROW( (account_name) ) }

   public_key_type wallet::get_active_public_key( const string& account_name )const
   { try {
      if( !my->_blockchain->is_valid_account_name( account_name ) )
          FC_THROW_EXCEPTION( invalid_name, "Invalid account name!", ("account_name",account_name) );
      FC_ASSERT( my->is_unique_account(account_name) );

      const auto registered_account = my->_blockchain->get_account_record( account_name );
      if( registered_account.valid() )
      {
         if( registered_account->is_retracted() )
             FC_CAPTURE_AND_THROW( account_retracted, (registered_account) );

         return registered_account->active_key();
      }

      FC_ASSERT( is_open() );
      const auto opt_account = my->_wallet_db.lookup_account( account_name );
      FC_ASSERT( opt_account.valid(), "Unable to find account '${name}'",
                ("name",account_name) );

      return opt_account->active_key();
   } FC_CAPTURE_AND_RETHROW( (account_name) ) }

   public_key_type wallet::get_owner_public_key( const string& account_name )const
   { try {
      if( !my->_blockchain->is_valid_account_name( account_name ) )
          FC_THROW_EXCEPTION( invalid_name, "Invalid account name!", ("account_name",account_name) );
      FC_ASSERT( my->is_unique_account(account_name) );

      const auto registered_account = my->_blockchain->get_account_record( account_name );
      if( registered_account.valid() )
      {
         if( registered_account->is_retracted() )
             FC_CAPTURE_AND_THROW( account_retracted, (registered_account) );

         return registered_account->owner_key;
      }

      FC_ASSERT( is_open() );
      const auto opt_account = my->_wallet_db.lookup_account( account_name );
      FC_ASSERT( opt_account.valid(), "Unable to find account '${name}'",
                ("name",account_name) );

      return opt_account->owner_key;
   } FC_CAPTURE_AND_RETHROW( (account_name) ) }

   void wallet::set_account_approval( const string& account_name, int8_t approval )
   { try {
      FC_ASSERT( is_open() );
      const auto account_record = my->_blockchain->get_account_record( account_name );
      auto war = my->_wallet_db.lookup_account( account_name );

      if( !account_record.valid() && !war.valid() )
          FC_THROW_EXCEPTION( unknown_account, "Unknown account name!", ("account_name",account_name) );

      if( war.valid() )
      {
         war->approved = approval;
         my->_wallet_db.store_account( *war );
         return;
      }

      add_contact_account( account_name, account_record->owner_key );
      set_account_approval( account_name, approval );
   } FC_CAPTURE_AND_RETHROW( (account_name)(approval) ) }

   int8_t wallet::get_account_approval( const string& account_name )const
   { try {
      FC_ASSERT( is_open() );
      const auto account_record = my->_blockchain->get_account_record( account_name );
      auto war = my->_wallet_db.lookup_account( account_name );

      if( !account_record.valid() && !war.valid() )
          FC_THROW_EXCEPTION( unknown_account, "Unknown account name!", ("account_name",account_name) );

      if( !war.valid() ) return 0;
      return war->approved;
   } FC_CAPTURE_AND_RETHROW( (account_name) ) }

   owallet_account_record wallet::get_account_for_address( address addr_in_account )const
   { try {
      FC_ASSERT( is_open() );
      const auto okey = my->_wallet_db.lookup_key( addr_in_account );
      if ( !okey.valid() ) return owallet_account_record();
      return my->_wallet_db.lookup_account( okey->account_address );
   } FC_CAPTURE_AND_RETHROW() }

   vector<escrow_summary>   wallet::get_escrow_balances( const string& account_name )
   {
      vector<escrow_summary> result;

      FC_ASSERT( is_open() );
      if( !account_name.empty() ) get_account( account_name ); /* Just to check input */

      map<string, vector<balance_record>> balance_records;
      const auto pending_state = my->_blockchain->get_pending_state();

      const auto scan_balance = [&]( const balance_record& record )
      {
          // check to see if it is a withdraw by escrow record
          if( record.condition.type == withdraw_escrow_type )
          {
             auto escrow_cond = record.condition.as<withdraw_with_escrow>();

             // lookup account for each key if known
             // lookup transaction that created the balance record in the local wallet
             // if the sender or receiver is one of our accounts and isn't filtered by account_name
             //    then add the escrow balance to the output.


             const auto sender_key_record = my->_wallet_db.lookup_key( escrow_cond.sender );
             const auto receiver_key_record = my->_wallet_db.lookup_key( escrow_cond.receiver );
             /*
             if( !((sender_key_record && sender_key_record->has_private_key()) ||
                   (receiver_key_record && receiver_key_record->has_private_key())) )
             {
                ilog( "no private key for sender nor receiver" );
                return; // no private key for the sender nor receiver
             }
             */
             escrow_summary sum;
             sum.balance_id = record.id();
             sum.balance    = record.get_spendable_balance( time_point_sec() );
             if( record.meta_data.is_object() )
                sum.creating_transaction_id = record.meta_data.get_object()["creating_transaction_id"].as<transaction_id_type>();

             if( sender_key_record )
             {
                const auto account_address = sender_key_record->account_address;
                const auto account_record = my->_wallet_db.lookup_account( account_address );
                if( account_record )
                {
                   const auto name = account_record->name;
                   sum.sender_account_name = name;
                }
                else
                {
                   auto registered_account = my->_blockchain->get_account_record( account_address );
                   if( registered_account )
                      sum.sender_account_name = registered_account->name;
                   else
                      sum.sender_account_name = string(escrow_cond.sender);
                }
             }
             else
             {
                sum.sender_account_name = string(escrow_cond.sender);
             }

             if( receiver_key_record )
             {
                const auto account_address = receiver_key_record->account_address;
                const auto account_record = my->_wallet_db.lookup_account( account_address );
                const auto name = account_record.valid() ? account_record->name : string( account_address );
                sum.receiver_account_name = name;
             }
             else
             {
                sum.receiver_account_name = "UNKNOWN";
             }

             auto agent_account = my->_blockchain->get_account_record( escrow_cond.escrow );
             if( agent_account )
             {
                if( agent_account->is_retracted() )
                   FC_CAPTURE_AND_THROW( account_retracted, (agent_account) );

                sum.escrow_agent_account_name = agent_account->name;
             }
             else
             {
                sum.escrow_agent_account_name = string( escrow_cond.escrow );
             }

             sum.agreement_digest = escrow_cond.agreement_digest;
             if( account_name.size() )
             {
                if( sum.sender_account_name == account_name ||
                    sum.receiver_account_name == account_name )
                {
                   result.emplace_back(sum);
                }
                else
                {
                  wlog( "skip ${s}", ("s",sum) );
                }
             }
             else result.emplace_back(sum);
          }
      };

      my->_blockchain->scan_balances( scan_balance );

      return result;
   }

   void wallet::scan_balances( const function<void( const balance_id_type&, const balance_record& )> callback )const
   {
       for( const auto& item : my->_balance_records )
           callback( item.first, item.second );
   }

   account_balance_record_summary_type wallet::get_spendable_account_balance_records( const string& account_name )const
   { try {
       map<string, vector<balance_record>> balances;
       const time_point_sec now = my->_blockchain->get_pending_state()->now();

       const auto scan_balance = [&]( const balance_id_type& id, const balance_record& record )
       {
           if( record.condition.type != withdraw_signature_type ) return;

           const asset balance = record.get_spendable_balance( now );
           if( balance.amount == 0 ) return;

           const optional<address> owner = record.owner();
           if( !owner.valid() ) return;

           const owallet_key_record key_record = my->_wallet_db.lookup_key( *owner );
           if( !key_record.valid() || !key_record->has_private_key() ) return;

           const owallet_account_record account_record = my->_wallet_db.lookup_account( key_record->account_address );
           const string name = account_record.valid() ? account_record->name : string( key_record->public_key );
           if( !account_name.empty() && name != account_name ) return;

           balances[ name ].push_back( record );
       };

       scan_balances( scan_balance );
       return balances;
   } FC_CAPTURE_AND_RETHROW( (account_name) ) }

   account_balance_summary_type wallet::get_spendable_account_balances( const string& account_name )const
   { try {
       map<string, map<asset_id_type, share_type>> balances;

       const map<string, vector<balance_record>> records = get_spendable_account_balance_records( account_name );
       const time_point_sec now = my->_blockchain->get_pending_state()->now();

       for( const auto& item : records )
       {
           const string& name = item.first;
           for( const balance_record& record : item.second )
           {
               const asset balance = record.get_spendable_balance( now );
               balances[ name ][ balance.asset_id ] += balance.amount;
           }
       }

       return balances;
   } FC_CAPTURE_AND_RETHROW( (account_name) ) }

   account_balance_id_summary_type wallet::get_account_balance_ids( const string& account_name )const
   { try {
       map<string, unordered_set<balance_id_type>> balances;

       const auto scan_balance = [&]( const balance_id_type& id, const balance_record& record )
       {
           const set<address>& owners = record.owners();
           for( const address& owner : owners )
           {
               const owallet_key_record key_record = my->_wallet_db.lookup_key( owner );
               if( !key_record.valid() || !key_record->has_private_key() ) continue;

               const owallet_account_record account_record = my->_wallet_db.lookup_account( key_record->account_address );
               const string name = account_record.valid() ? account_record->name : string( key_record->public_key );
               if( !account_name.empty() && name != account_name ) continue;

               balances[ name ].insert( id );
           }
       };

       scan_balances( scan_balance );
       return balances;
   } FC_CAPTURE_AND_RETHROW( (account_name) ) }

   account_vesting_balance_summary_type wallet::get_account_vesting_balances( const string& account_name )const
   { try {
       map<string, vector<pretty_vesting_balance>> balances;
       const time_point_sec now = my->_blockchain->get_pending_state()->now();

       const auto scan_balance = [&]( const balance_id_type& id, const balance_record& record )
       {
           if( record.condition.type != withdraw_vesting_type ) return;

           const optional<address> owner = record.owner();
           if( !owner.valid() ) return;

           const owallet_key_record key_record = my->_wallet_db.lookup_key( *owner );
           if( !key_record.valid() || !key_record->has_private_key() ) return;

           const owallet_account_record account_record = my->_wallet_db.lookup_account( key_record->account_address );
           const string name = account_record.valid() ? account_record->name : string( key_record->public_key );
           if( !account_name.empty() && name != account_name ) return;

           const withdraw_vesting& condition = record.condition.as<withdraw_vesting>();
           pretty_vesting_balance balance;
           balance.balance_id = id;
           if( record.snapshot_info.valid() ) balance.sharedrop_address = record.snapshot_info->original_address;
           balance.start_time = condition.start_time;
           balance.duration = condition.duration;
           balance.asset_id = record.asset_id();
           balance.original_balance = condition.original_balance;
           balance.claimed_balance = balance.original_balance - record.balance;
           balance.available_balance = record.get_spendable_balance( now ).amount;
           balance.vested_balance = balance.claimed_balance + balance.available_balance;

           balances[ name ].push_back( std::move( balance ) );
       };

       scan_balances( scan_balance );
       return balances;
   } FC_CAPTURE_AND_RETHROW( (account_name) ) }

   account_balance_summary_type wallet::get_account_yield( const string& account_name )const
   { try {
      map<string, map<asset_id_type, share_type>> yield_summary;
      const time_point_sec now = my->_blockchain->get_pending_state()->now();

      map<string, vector<balance_record>> items = get_spendable_account_balance_records( account_name );
      for( const auto& item : items )
      {
          const auto& name = item.first;
          const auto& records = item.second;

          for( const auto& record : records )
          {
              const auto balance = record.get_spendable_balance( now );
              // TODO: Memoize these
              const auto asset_rec = my->_blockchain->get_asset_record( balance.asset_id );
              if( !asset_rec.valid() || !asset_rec->is_market_issued() ) continue;

              const auto yield = record.calculate_yield( now, balance.amount, asset_rec->collected_fees,
                                                         asset_rec->current_share_supply );
              yield_summary[ name ][ yield.asset_id ] += yield.amount;
          }
      }

      return yield_summary;
   } FC_CAPTURE_AND_RETHROW( (account_name) ) }

   account_vote_summary_type wallet::get_account_vote_summary( const string& account_name )const
   { try {
      const auto pending_state = my->_blockchain->get_pending_state();
      auto raw_votes = map<account_id_type, int64_t>();
      auto result = account_vote_summary_type();

      const account_balance_record_summary_type items = get_spendable_account_balance_records( account_name );
      for( const auto& item : items )
      {
          const auto& records = item.second;
          for( const auto& record : records )
          {
              const auto owner = record.owner();
              if( !owner.valid() ) continue;

              const auto okey_rec = my->_wallet_db.lookup_key( *owner );
              if( !okey_rec.valid() || !okey_rec->has_private_key() ) continue;

              const auto oaccount_rec = my->_wallet_db.lookup_account( okey_rec->account_address );
              if( !oaccount_rec.valid() ) FC_THROW_EXCEPTION( unknown_account, "Unknown account name!" );
              if( !account_name.empty() && oaccount_rec->name != account_name ) continue;

              const auto obalance = pending_state->get_balance_record( record.id() );
              if( !obalance.valid() ) continue;

              const auto balance = obalance->get_spendable_balance( pending_state->now() );
              if( balance.amount <= 0 || balance.asset_id != 0 ) continue;

              const auto slate_id = obalance->slate_id();
              if( slate_id == 0 ) continue;

              const auto slate_record = pending_state->get_slate_record( slate_id );
              if( !slate_record.valid() ) FC_THROW_EXCEPTION( unknown_slate, "Unknown slate!" );

              for( const account_id_type delegate_id : slate_record->slate )
              {
                  if( raw_votes.count( delegate_id ) <= 0 ) raw_votes[ delegate_id ] = balance.amount;
                  else raw_votes[ delegate_id ] += balance.amount;
              }
          }
      }

      for( const auto& item : raw_votes )
      {
         auto delegate_account = pending_state->get_account_record( item.first );
         result[ delegate_account->name ] = item.second;
      }

      return result;
   } FC_CAPTURE_AND_RETHROW() }

   variant wallet::get_info()const
   {
       const time_point_sec now = blockchain::now();
       mutable_variant_object info;

       info["data_dir"]                                 = fc::absolute( my->_data_directory );
       info["num_scanning_threads"]                     = my->_num_scanner_threads;

       const auto is_open                               = this->is_open();
       info["open"]                                     = is_open;

       info["name"]                                     = variant();
       info["automatic_backups"]                        = variant();
       info["transaction_scanning_enabled"]             = variant();
       info["last_scanned_block_num"]                   = variant();
       info["last_scanned_block_timestamp"]             = variant();
       info["transaction_fee"]                          = variant();
       info["transaction_expiration_secs"]              = variant();

       info["unlocked"]                                 = variant();
       info["unlocked_until"]                           = variant();
       info["unlocked_until_timestamp"]                 = variant();

       info["scan_progress"]                            = variant();

       info["version"]                                  = variant();

       if( is_open )
       {
         info["name"]                                   = my->_current_wallet_path.filename().string();
         info["automatic_backups"]                      = get_automatic_backups();
         info["transaction_scanning_enabled"]           = get_transaction_scanning();

         const auto last_scanned_block_num              = get_last_scanned_block_number();
         if( last_scanned_block_num > 0 )
         {
             info["last_scanned_block_num"]             = last_scanned_block_num;
             try
             {
                 info["last_scanned_block_timestamp"]   = my->_blockchain->get_block_header( last_scanned_block_num ).timestamp;
             }
             catch( ... )
             {
             }
         }

         info["transaction_fee"]                        = get_transaction_fee();
         info["transaction_expiration_secs"]            = get_transaction_expiration();

         info["unlocked"]                               = is_unlocked();

         const auto unlocked_until                      = this->unlocked_until();
         if( unlocked_until.valid() )
         {
           info["unlocked_until"]                       = ( *unlocked_until - now ).to_seconds();
           info["unlocked_until_timestamp"]             = *unlocked_until;

           info["scan_progress"]                        = get_scan_progress();
         }

         info["version"]                                = get_version();
       }

       return info;
   }

   public_key_summary wallet::get_public_key_summary( const public_key_type& pubkey ) const
   {
       public_key_summary summary;
       summary.hex = variant( fc::ecc::public_key_data(pubkey) ).as_string();
       summary.native_pubkey = string( pubkey );
       summary.native_address = string( address( pubkey ) );
       summary.pts_normal_address = string( pts_address( pubkey, false, 56 ) );
       summary.pts_compressed_address = string( pts_address( pubkey, true, 56 ) );
       summary.btc_normal_address = string( pts_address( pubkey, false, 0 ) );
       summary.btc_compressed_address = string( pts_address( pubkey, true, 0 ) );
       return summary;
   }

   vector<public_key_type> wallet::get_public_keys_in_account( const string& account_name )const
   {
      const auto account_rec = my->_wallet_db.lookup_account( account_name );
      if( !account_rec.valid() )
          FC_THROW_EXCEPTION( unknown_account, "Unknown account name!" );

      const auto account_address = address( get_owner_public_key( account_name ) );

      vector<public_key_type> account_keys;
      const auto keys = my->_wallet_db.get_keys();
      for( const auto& key : keys )
      {
         if( key.second.account_address == account_address || key.first == account_address )
            account_keys.push_back( key.second.public_key );
      }
      return account_keys;
   }


   map<order_id_type, market_order> wallet::get_market_orders( const string& account_name, const string& quote_symbol,
                                                               const string& base_symbol, uint32_t limit )const
   { try {
      map<order_id_type, market_order> order_map;

      const oasset_record quote_record = my->_blockchain->get_asset_record( quote_symbol );
      const oasset_record base_record = my->_blockchain->get_asset_record( base_symbol );
      FC_ASSERT( quote_symbol.empty() || quote_record.valid() );
      FC_ASSERT( base_symbol.empty() || base_record.valid() );

      const auto filter = [&]( const market_order& order ) -> bool
      {
          if( quote_record.valid() && order.market_index.order_price.quote_asset_id != quote_record->id )
              return false;

          if( base_record.valid() && order.market_index.order_price.base_asset_id != base_record->id )
              return false;

          const owallet_key_record key_record = my->_wallet_db.lookup_key( order.get_owner() );
          if( !key_record.valid() || !key_record->has_private_key() )
              return false;

          if( !account_name.empty() )
          {
              const owallet_account_record account_record = my->_wallet_db.lookup_account( key_record->account_address );
              if( !account_record.valid() || account_record->name != account_name )
                  return false;
          }

          return true;
      };

      const auto orders = my->_blockchain->scan_market_orders( filter, limit );
      for( const auto& order : orders )
          order_map[ order.get_id() ] = order;

      return order_map;
   } FC_CAPTURE_AND_RETHROW( (account_name)(quote_symbol)(base_symbol)(limit) ) }

   void wallet::write_latest_builder( const transaction_builder& builder,
                                      const string& alternate_path )
   {
        std::ofstream fs;
        if( alternate_path == "" )
        {
            auto dir = (get_data_directory() / "trx").string();
            auto default_path = dir + "/latest.trx";
            if( !fc::exists( default_path ) )
                fc::create_directories( dir );
            fs.open(default_path);
        }
        else
        {
            if( fc::exists( alternate_path  ) )
                FC_THROW_EXCEPTION( file_already_exists, "That filename already exists!", ("filename", alternate_path));
            fs.open(alternate_path);
        }
        fs << fc::json::to_pretty_string(builder);
        fs.close();
   }

   wallet_transaction_record wallet::asset_authorize_key( const string& paying_account_name,
                                                  const string& symbol,
                                                  const address& key,
                                                  const object_id_type meta, bool sign )
   {
      if( NOT is_open()     ) FC_CAPTURE_AND_THROW( wallet_closed );
      if( NOT is_unlocked() ) FC_CAPTURE_AND_THROW( wallet_locked );
      auto payer_key = get_owner_public_key( paying_account_name );

      transaction_builder_ptr builder = create_transaction_builder();
      builder->asset_authorize_key( symbol, key, meta );
      builder->deduct_balance( payer_key, asset() );
      builder->finalize();

      if( sign )
         return builder->sign();
      return builder->transaction_record;
   }

   void wallet::initialize_transaction_creator( transaction_creation_state& c, const string& account_name )
   {
      c.pending_state._balance_id_to_record = my->_balance_records;
      vector<public_key_type>  keys = get_public_keys_in_account( account_name );
      for( auto key : keys ) c.add_known_key( key );
   }

   void wallet::sign_transaction_creator( transaction_creation_state& c )
   {

   }

} } // bts::wallet
