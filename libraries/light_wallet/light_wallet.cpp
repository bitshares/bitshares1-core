#include <bts/light_wallet/light_wallet.hpp>
#include <bts/blockchain/time.hpp>
#include <bts/blockchain/balance_operations.hpp>
#include <bts/blockchain/transaction_creation_state.hpp>
#include <bts/utilities/key_conversion.hpp>

#include <fc/network/resolve.hpp>
#include <fc/crypto/aes.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/io/json.hpp>

namespace bts { namespace light_wallet {
using std::string;

light_wallet::light_wallet(std::function<void(string,string)> persist_function, std::function<string(string)> restore_function, std::function<bool(string)> can_restore_function)
   : persist(persist_function),
     can_restore(can_restore_function),
     restore(restore_function)
{
   try {
      int storage_version = 0;
      if( can_restore("storage_version") )
         storage_version = std::stoi(restore("storage_version"));
      if( storage_version != BTS_LIGHT_WALLET_STORAGE_VERSION )
      {
         // STORAGE VERSION UPGRADES GO HERE

         if( storage_version == 0 && can_restore("wallet_data") ) {
            //Remove scanned transaction cache
            wlog("Upgrading storage from version 0 to version 1.");
            light_wallet_data data = fc::json::from_string(restore("wallet_data"), fc::json::strict_parser).as<light_wallet_data>();
            for( auto& account : data.accounts )
               account.second.scan_cache.clear();
            persist("wallet_data", fc::json::to_string(data));
            storage_version = 1;
         }
      }
      persist("storage_version", fc::to_string(BTS_LIGHT_WALLET_STORAGE_VERSION));

      _chain_cache = std::make_shared<pending_chain_state>();
      if( can_restore("chain_cache") )
         *_chain_cache = fc::json::from_string(restore("chain_cache"), fc::json::strict_parser).as<pending_chain_state>();
   } catch ( const fc::exception& e )
   {
      elog( "Error loading chain cache: ${e}", ("e", e.to_detail_string()));
   }
}

light_wallet::~light_wallet()
{
   try {
      persist("chain_cache", fc::json::to_string(*_chain_cache));
      _chain_cache.reset();
   } catch( const fc::exception& e) {
      edump((e));
   }
}

void light_wallet::fetch_welcome_package()
{
   fc::mutable_variant_object args;
   asset_id_type last_known_asset_id = -1;
   //_asset_id_to_record is unordered_map, so not sorted
   for( const auto& asset : _chain_cache->_asset_id_to_record )
      last_known_asset_id = std::max(last_known_asset_id, asset.first);
   args["last_known_asset_id"] = last_known_asset_id;

   auto welcome_package = _rpc.fetch_welcome_package(args);
   _chain_cache->set_chain_id(welcome_package["chain_id"].as<digest_type>());
   _relay_fee_collector = welcome_package["relay_fee_collector"].as<oaccount_record>();
   _relay_fee = asset(welcome_package["relay_fee_amount"].as<share_type>());
   _network_fee = asset(welcome_package["network_fee_amount"].as<share_type>());

   for( auto& record : welcome_package["new_assets"].as<vector<asset_record>>() )
   {
      //Caveat: we need to already have cached the issuer's account if dealing with a UIA
      if( record.is_user_issued() )
         get_account_record(fc::to_string(record.issuer_id));
      _chain_cache->store_asset_record(std::move(record));
   }
}

void light_wallet::connect(const string& host, const string& user, const string& pass, uint16_t port,
                            const public_key_type& server_key)
{
   string last_error;
   auto resolved = fc::resolve( host, port ? port : BTS_LIGHT_WALLET_PORT );
   if( resolved.empty() ) last_error = "Unable to resolve host: " + host;

   for( auto item : resolved )
   {
      try {
         _rpc.connect_to( item, blockchain::public_key_type(server_key) );
         set_disconnect_callback(nullptr);
         if( user != "any" && pass != "none" )
            _rpc.login( user, pass );

         fetch_welcome_package();
         _is_connected = true;

         return;
      }
      catch ( const fc::exception& e )
      {
         wlog( "Connection Failed ${f}", ("f",e.to_detail_string()) );
         last_error = e.to_string();
      }
   }
   FC_THROW( "Unable to connect: ${e}", ("e", last_error) );
}

bool light_wallet::is_connected() const
{
   return _is_connected;
}

void light_wallet::set_disconnect_callback(std::function<void(fc::exception_ptr)> callback)
{
   auto connection = _rpc.get_json_connection();
   if( !connection ) return;
   _rpc.get_json_connection()->set_on_disconnected_callback([this, callback](fc::exception_ptr e) {
      _is_connected = false;
      if( callback )
         callback(e);
   });
}

void light_wallet::disconnect()
{
   _rpc.reset_json_connection();
}

void light_wallet::open()
{
   _data = light_wallet_data();
   if( can_restore("wallet_data") )
      _data = fc::json::from_string(restore("wallet_data"), fc::json::strict_parser).as<light_wallet_data>();
}

void light_wallet::save()
{
   if( _data )
      persist("wallet_data", fc::json::to_string(*_data));
}

void light_wallet::close()
{
   lock();
   _data.reset();
}

void light_wallet::unlock( const string& password )
{ try {
   FC_ASSERT( is_open() );
   if( !_wallet_key )
   {
      auto key = fc::sha512::hash( password );
      //How do I verify this if there are no accounts?
      if( !_data->accounts.empty() )
      {
         _wallet_key = key;
         //Should throw if decryption fails
         try {
            active_key(_data->accounts.begin()->first);
         } catch(...) {
            _wallet_key.reset();
            throw;
         }
      }
   }
} FC_CAPTURE_AND_RETHROW() }

bool light_wallet::is_unlocked()const { return _wallet_key.valid(); }
bool light_wallet::is_open()const { return _data.valid(); }

void light_wallet::change_password( const string& new_password )
{ try {
   FC_ASSERT( is_open() );
   FC_ASSERT( is_unlocked() );
   auto pass_key = fc::sha512::hash( new_password );

   for( auto& account_pair : _data->accounts )
      account_pair.second.encrypted_private_key = fc::aes_encrypt(pass_key, fc::raw::pack(active_key(account_pair.first)));

   _wallet_key = pass_key;
   save();
} FC_CAPTURE_AND_RETHROW() }

void light_wallet::lock()
{
   if( _wallet_key )
      *_wallet_key = fc::sha512();
   _wallet_key.reset();
}

fc::ecc::private_key light_wallet::derive_private_key(const string& prefix_string, int sequence_number)
{
   string sequence_string = std::to_string(sequence_number);
   fc::sha512 hash = fc::sha512::hash(prefix_string + " " + sequence_string);
   fc::ecc::private_key owner_key = fc::ecc::private_key::regenerate(fc::sha256::hash(hash));

   return owner_key;
}

void light_wallet::create( const string& account_name,
                           const string& password,
                           const string& brain_seed )
{
   // initialize the wallet data
   if( !_data.valid() )
      _data = light_wallet_data();
   account_record& new_account = _data->accounts[account_name].user_account;

   int n = 0;
   fc::ecc::private_key owner_key;
   do {
      // Don't actually store the owner key; it can be recovered via brain key and salt.
      // Locally, we'll only keep a deterministically derived child key so if that key is compromised,
      // the owner key remains safe.
      // Owner key is hash(brain_seed + n) where n = number of previous accounts on this key. Keep trying subsequent
      // values of n until we can't find an account with that key.
      owner_key = derive_private_key(brain_seed, n++);
      new_account.owner_key = owner_key.get_public_key();
   } while( get_account_record(string(blockchain::public_key_type(owner_key.get_public_key()))).valid() &&
            get_account_record(string(blockchain::public_key_type(owner_key.get_public_key())))->name != account_name );

   // Active key is hash(wif_owner_key + " " + n) where n = number of previous active keys
   // i.e. first active key has n=0, second has n=1, etc. We're creating a new account, so n=0
   fc::ecc::private_key active_key = derive_private_key(utilities::key_to_wif(owner_key), 0);
   new_account.active_key_history[fc::time_point::now()] = active_key.get_public_key();

   // set the password
   _wallet_key = fc::sha512::hash( password );
   _data->accounts[account_name].encrypted_private_key = fc::aes_encrypt( *_wallet_key, fc::raw::pack( active_key ) );
   new_account.name = account_name;
   new_account.meta_data = account_meta_info( public_account );
   save();
}

bool light_wallet::request_register_account(const string& account_name)
{ try {
   FC_ASSERT( is_open() );
   try {
      return _rpc.request_register_account( _data->accounts[account_name].user_account );
   } catch (const fc::exception& e) {
      //If server gave back an account_already_registered, check if it's actually my account.
      if( e.code() == account_already_registered().code() &&
          _rpc.blockchain_get_account(_data->accounts[account_name].user_account.name)->active_key() != _data->accounts[account_name].user_account.active_key() )
         throw;
      return false;
   }
} FC_CAPTURE_AND_RETHROW( ) }

account_record& light_wallet::account(const string& account_name)
{ try {
   FC_ASSERT( is_open() );
   FC_ASSERT( _data->accounts.count(account_name) );
   return _data->accounts[account_name].user_account;
} FC_CAPTURE_AND_RETHROW( ) }

account_record& light_wallet::fetch_account(const string& account_name)
{ try {
   FC_ASSERT( is_open() );
   FC_ASSERT( _data->accounts.count(account_name) );
   auto account = _rpc.blockchain_get_account(account_name);
   if( account )
   {
      if( account->active_key() != _data->accounts[account_name].user_account.active_key() )
         FC_THROW_EXCEPTION( account_already_registered,
                             "Attempted to fetch my account, but got one with an unknown active key.",
                             ("my_account", _data->accounts[account_name].user_account)("blockchain_account", account) );
      _data->accounts[account_name].user_account = *account;
      save();
   }

   return _data->accounts[account_name].user_account;
   } FC_CAPTURE_AND_RETHROW( ) }

vector<const account_record*> light_wallet::account_records()const
{
   FC_ASSERT(is_open());
   vector<const account_record*> results;

   for( auto& account_pair : _data->accounts )
      results.push_back(&account_pair.second.user_account);

   return results;
}

fc::variant_object light_wallet::prepare_transfer(const string& amount,
                                                  const string& symbol,
                                                  const string& from_account_name,
                                                  const string& to_account_name,
                                                  const string& memo )
{ try {
   FC_ASSERT( is_unlocked() );
   FC_ASSERT( _data->accounts.count(from_account_name) );

   auto symbol_asset = get_asset_record( symbol );
   FC_ASSERT( symbol_asset.valid() );
   asset transfer_asset = symbol_asset->asset_from_string(amount);

   auto to_account  = get_account_record( to_account_name );
   bts::blockchain::public_key_type recipient_key;
   if( to_account.valid() )
      recipient_key = to_account->active_key();
   else
      recipient_key = bts::blockchain::public_key_type(to_account_name);

   asset fee = get_fee( symbol );

   fc::time_point_sec expiration = fc::time_point::now() + fc::seconds(BTS_BLOCKCHAIN_BLOCK_INTERVAL_SEC * 30);

   bts::blockchain::transaction_creation_state creator;
   creator.pending_state = *_chain_cache;
   creator.add_known_key(_data->accounts[from_account_name].user_account.active_address());
   if( fee.asset_id == transfer_asset.asset_id )
      creator.withdraw(fee + transfer_asset);
   else {
      creator.withdraw(fee);
      creator.withdraw(transfer_asset);
   }

   if( _relay_fee_collector )
   {
      asset relay_fee = fee - get_network_fee(symbol);
      creator.deposit(relay_fee, _relay_fee_collector->active_key(), 0);
   }

   creator.deposit(transfer_asset, recipient_key, 0,
                   create_one_time_key(from_account_name, fc::to_string(expiration.sec_since_epoch())),
                   memo, active_key(from_account_name));

   //creator.pay_fee(fee);

   creator.trx.expiration = expiration;

   ilog("Creator built a transaction: ${trx}", ("trx", creator.trx));

   // TODO support multi-sig balances through service provider which
   // will perform N factor authentication

   transaction_record record(transaction_location(), creator.eval_state);
   record.trx = creator.trx;
   fc::mutable_variant_object result("trx", record);
   result["timestamp"] = fc::time_point_sec(fc::time_point::now());
   return result;
} FC_CAPTURE_AND_RETHROW( (amount)(symbol)(to_account_name)(memo) ) }

bool light_wallet::complete_transfer(const string& account_name,
                                     const string& password,
                                     const fc::variant_object& transaction_bundle )
{
   try {
      if( !is_unlocked() )
      {
         unlock(password);
      } else {
         auto pass_key = fc::sha512::hash( password );
         if( pass_key != *_wallet_key )
            return false;
      }
   } catch(...) {
      return false;
   }

   signed_transaction trx = transaction_bundle["trx"].as<transaction_record>().trx;
   trx.sign(active_key(account_name), _chain_cache->get_chain_id());
   try {
      _rpc.blockchain_broadcast_transaction(trx);
   } catch (const fc::exception& e) {
      if( e.to_detail_string().find("charity") != string::npos )
         //Insufficient light server relay fee... Why would this happen? We should've paid it.
         FC_THROW("Unable to complete transaction. Please try again later.");
   }

   return true;
}

oprice light_wallet::get_median_feed_price( const string& symbol )
{
   if( is_open() )
   {
      auto cached_price_itr = _data->price_cache.find(symbol);
      if( cached_price_itr != _data->price_cache.end() )
      {
         if( cached_price_itr->second.second + fc::hours(1) > fc::time_point::now() )
            return cached_price_itr->second.first;
         else
            _data->price_cache.erase( cached_price_itr );
      }
   }
   const string price_ratio = _rpc.blockchain_median_feed_price( symbol );
   oprice opt;
   if( price_ratio != "" )
   {
      auto base_rec = get_asset_record( BTS_BLOCKCHAIN_SYMBOL );
      auto quote_rec = get_asset_record( symbol );
      FC_ASSERT( base_rec && quote_rec );

      opt = price();
      opt->base_asset_id = base_rec->id;
      opt->quote_asset_id = quote_rec->id;
      opt->set_ratio_from_string( price_ratio );
   }

   if( opt && is_open() )
   {
      _data->price_cache[symbol] = std::make_pair( *opt, fc::time_point::now() );
   }
   return opt;
}

asset light_wallet::get_fee( const string& symbol )
{ try {
   auto symbol_asset = get_asset_record(symbol);
   if( symbol_asset.valid() && symbol_asset->id != 0 )
   {
      oprice median_feed = get_median_feed_price( symbol );
      if( median_feed )
         return (_network_fee + _relay_fee) * *median_feed;
   }
   return _network_fee + _relay_fee;
} FC_CAPTURE_AND_RETHROW( (symbol) ) }

map<string, pair<double,double>> light_wallet::balance(const string& account_name) const
{
   FC_ASSERT(is_open());

   map<string, pair<double,double>> balances = {{BTS_BLOCKCHAIN_SYMBOL, {0,0}}};
   for( auto balance : _chain_cache->_balance_id_to_record ) {
      oasset_record record = get_asset_record(balance.second.asset_id());
      if( record && balance.second.owner() == _data->accounts.find(account_name)->second.user_account.active_key() )
      {
         balances[record->symbol].first += balance.second.balance / double(record->precision);
         balances[record->symbol].second += balance.second.calculate_yield(blockchain::now(),
                                                                           balance.second.balance,
                                                                           record->collected_fees,
                                                                           record->current_supply).amount / double(record->precision);
      }
   }
   return balances;
}

vector<bts::wallet::transaction_ledger_entry> light_wallet::transactions(const string& account_name,
                                                                         const string& symbol)
{
   FC_ASSERT( is_open() );
   auto asset_rec = get_asset_record(symbol);
   FC_ASSERT(asset_rec.valid());

   vector<bts::wallet::transaction_ledger_entry> results;
   auto ids = _data->accounts[account_name].transaction_index.equal_range(asset_rec->id);
   std::for_each(ids.first, ids.second, [this, &results, &account_name](const std::pair<asset_id_type, transaction_id_type>& id) {
      results.emplace_back(summarize(account_name, _data->accounts[account_name].transaction_record_cache[id.second]));
   });

   return results;
}


bool light_wallet::sync_balance( bool resync_all )
{ try {
   FC_ASSERT( is_open() );
   static bool first_time = true;
   std::vector<fc::variants> batch_args;

   if( first_time )
   {
      //Refresh old balances
      for( auto balance : _chain_cache->_balance_id_to_record )
         batch_args.push_back(fc::variants(1, fc::variant(balance.first)));

      try {
         auto balance_records = _rpc.batch("blockchain_get_balance", batch_args);
         for( const auto& rec : balance_records )
            _chain_cache->store_balance_record(rec.as<balance_record>());
      } catch (...) {
         resync_all = true;
      }

      first_time = false;
   }

   if( resync_all )
   {
     _data->last_balance_sync_time = fc::time_point_sec();
     _chain_cache->_balance_id_to_record.clear();
   }

   if( _data->last_balance_sync_time + fc::seconds(10) > fc::time_point::now() )
      return false; // too fast

   fc::time_point_sec sync_time = _data->last_balance_sync_time;
   vector<string> account_names;

   auto batch_results = batch_active_addresses("blockchain_list_address_balances", fc::variant(sync_time), account_names);

   if( batch_results.empty() || batch_results.begin()->as<map<balance_id_type, balance_record>>().empty() )
      return false;

   batch_args.clear();
   for( const auto& new_balances : batch_results )
      for( auto item : new_balances.as<map<balance_id_type, balance_record>>() )
      {
         _chain_cache->store_balance_record(item.second);
         batch_args.push_back(fc::variants(1, fc::variant(item.second.asset_id())));

         sync_time = std::max(item.second.last_update, sync_time);
      }

   auto asset_records = _rpc.batch("blockchain_get_asset", batch_args);
   for( int i = 0; i < asset_records.size(); ++i )
      _chain_cache->store_asset_record(asset_records[i].as<asset_record>());

   _data->last_balance_sync_time = sync_time;
   save();
   return true;
} FC_CAPTURE_AND_RETHROW() }

bool light_wallet::sync_transactions(bool resync_all)
{
   FC_ASSERT( is_open() );

   uint32_t sync_block = _data->last_transaction_sync_block;
   if( resync_all )
      sync_block = 0;
   vector<string> account_names;

   auto batch_results = batch_active_addresses("blockchain_list_address_transactions", sync_block, account_names);

   if( batch_results.empty() )
      return false;

   for( int i = 0; i < batch_results.size(); ++i )
   {
      const string& account_name = account_names[i];
      fc::variant_object new_trxs = batch_results[i].as<fc::variant_object>();

      for( const auto& item : new_trxs )
      {
         transaction_id_type id(item.key());
         fc::variant_object bundle = item.value().as<fc::variant_object>();
         transaction_record record = bundle["trx"].as<transaction_record>();
         _data->accounts[account_name].transaction_record_cache[id] = bundle;

         sync_block = std::max(record.chain_location.block_num, sync_block);

         wallet::transaction_ledger_entry scanned_trx = summarize(account_name, bundle);
         for( const auto& delta : scanned_trx.delta_amounts )
            for( const asset& balance : delta.second )
            {
               auto& index = _data->accounts[account_name].transaction_index;
               //if this <asset id, trx id> pair is not already in the transaction_index, add it.
               if( std::find_if(index.begin(), index.end(),
                                [balance, id](const std::pair<asset_id_type, transaction_id_type>& entry) -> bool {
                                   return entry.first == balance.asset_id && entry.second == id;
                   })
                   == index.end() )
                  index.insert({balance.asset_id, id});
            }
      }
   }
   _data->last_transaction_sync_block = sync_block;
   save();
   return true;
}

fc::variants light_wallet::batch_active_addresses(const char* call_name, fc::variant last_sync, vector<string>& account_names)
{
   vector<fc::variants> batch_args;
   for( const auto& account_pair : _data->accounts )
   {
      const string& account_name = account_pair.first;
      batch_args.push_back({string(address(_data->accounts[account_name].user_account.active_key())),
                            last_sync});
      account_names.emplace_back(std::move(account_name));
   }

   auto batch_results = _rpc.batch(call_name, batch_args);

   return batch_results;
}

fc::ecc::private_key light_wallet::active_key(const string& account_name)
{
   FC_ASSERT(is_open());
   FC_ASSERT(is_unlocked());
   return fc::raw::unpack<fc::ecc::private_key>(fc::aes_decrypt(*_wallet_key,
                                                                _data->accounts[account_name].encrypted_private_key));
}

optional<asset_record> light_wallet::get_asset_record( const string& symbol )const
{ try {
   if( is_open() )
   {
      for( auto item : _chain_cache->_asset_id_to_record)
         if( item.second.symbol == symbol )
            return item.second;
   }
   if( is_connected() )
   {
      auto result = _rpc.blockchain_get_asset( symbol );
      if( result )
         _chain_cache->store_asset_record(*result);
      return result;
   }
   return oasset_record();
} FC_CAPTURE_AND_RETHROW( (symbol) ) }

optional<asset_record> light_wallet::get_asset_record(const asset_id_type& id)const
{ try {
   if( is_open() )
   {
      for( auto item : _chain_cache->_asset_id_to_record)
         if( item.second.id == id )
            return item.second;
   }
   if( is_connected() )
   {
      auto result = _rpc.blockchain_get_asset( fc::variant(id).as_string() );
      if( result )
         _chain_cache->store_asset_record(*result);
      return result;
   }
   return oasset_record();
   } FC_CAPTURE_AND_RETHROW( (id) ) }

vector<string> light_wallet::all_asset_symbols() const
{
   vector<string> symbols;

   if( _chain_cache )
      for( const auto& rec : _chain_cache->_asset_id_to_record )
         symbols.emplace_back(rec.second.symbol);

   return symbols;
}

oaccount_record light_wallet::get_account_record(const string& identifier)
{
   auto itr = _account_cache.find(identifier);
   if( itr != _account_cache.end() )
      return itr->second;
   wlog("Cache miss on account ${a}", ("a", identifier));

   if( is_connected() )
   {
      auto account = _rpc.blockchain_get_account(identifier);
      if( account )
      {
         _chain_cache->store_account_record(*account);
         _account_cache[account->name] = *account;
         _account_cache[string(account->owner_key)] = *account;
         _account_cache[string(account->active_key())] = *account;
         _account_cache[string(account->owner_address())] = *account;
         _account_cache[string(account->active_address())] = *account;
      }
      return account;
   }
   return oaccount_record();
}

bts::wallet::transaction_ledger_entry light_wallet::summarize(const string& account_name,
                                                              const fc::variant_object& transaction_bundle)
{ try {
   FC_ASSERT( is_open() );
   FC_ASSERT( is_unlocked() );

   map<string, map<asset_id_type, share_type>> raw_delta_amounts;
   bts::wallet::transaction_ledger_entry summary;
   transaction_record record = transaction_bundle["trx"].as<transaction_record>();
   summary.timestamp = transaction_bundle["timestamp"].as<fc::time_point_sec>();
   summary.block_num = record.chain_location.block_num;
   summary.id = record.trx.id();
   auto& cache = _data->accounts[account_name].scan_cache;

   auto itr = cache.find(summary.id);
   if( itr != cache.end() )
   {
      ilog("Using cached summary: ${s}", ("s", itr->second));
      return itr->second;
   }

   ilog("Summarizing ${t}", ("t", transaction_bundle));

   bool tally_fees = false;

   for( int i = 0; i < record.trx.operations.size(); ++i )
   {
      auto op = record.trx.operations[i];
      switch( operation_type_enum(op.type) )
      {
      case deposit_op_type: {
         deposit_operation deposit = op.as<deposit_operation>();
         auto asset_id = deposit.condition.asset_id;
         if( deposit.condition.type == withdraw_signature_type )
         {
            withdraw_with_signature condition = deposit.condition.as<withdraw_with_signature>();
            if( condition.owner == _data->accounts[account_name].user_account.active_address() )
            {
               //It's to me.
               summary.delta_labels[i] = "Unknown>";
               if( condition.memo )
               {
                  omemo_status status = condition.decrypt_memo_data(active_key(account_name), true);
                  if( status )
                  {
                     summary.operation_notes[i] = status->get_message();
                     auto sender = get_account_record(string(status->from));
                     if( sender )
                        summary.delta_labels[i] = sender->name + ">";
                     else
                        summary.delta_labels[i] = string(status->from) + ">";
                  }
               }
               summary.delta_labels[i] += _data->accounts[account_name].user_account.name;

               raw_delta_amounts[summary.delta_labels[i]][asset_id] += deposit.amount;
            } else {
               //It's from me.
               tally_fees = true;
               //Is it a light relay fee?
               if( _relay_fee_collector && condition.owner == _relay_fee_collector->active_address() && !condition.memo )
               {
                  //Tally it up as fees, and don't show it in ledger
                  record.fees_paid[deposit.condition.asset_id] += deposit.amount;
                  continue;
               }

               summary.delta_labels[i] = string(condition.owner);

               //Do I know the recipient? If so, name him
               auto account = get_account_record(string(condition.owner));
               if( account ) summary.delta_labels[i] = _data->accounts[account_name].user_account.name + ">" + account->name;
               else summary.delta_labels[i] = _data->accounts[account_name].user_account.name + ">" + "Unregistered Account";

               //Recover memo
               if( account && condition.memo )
               {
                  auto one_time_key = create_one_time_key(account_name, fc::to_string(record.trx.expiration.sec_since_epoch()));
                  try {
                     extended_memo_data data = condition.decrypt_memo_data(one_time_key.get_shared_secret(account->active_key()));
                     summary.operation_notes[i] = data.get_message();
                  } catch(...){}
               }

               //Record any yield
               if( record.yield_claimed.count(asset_id) )
               {
                  auto& yields = summary.delta_amounts["Yield"];
                  auto asset_yield = std::find_if(yields.begin(), yields.end(),
                                                  [asset_id] (const asset& a) {
                                                     return a.asset_id == asset_id;
                                                  });
                  if( asset_yield != yields.end() )
                     asset_yield->amount += record.yield_claimed[asset_id];
                  else
                     yields.emplace_back(record.yield_claimed[asset_id], asset_id);
               }

               raw_delta_amounts[summary.delta_labels[i]][asset_id] -= deposit.amount;
            }
         }
         break;
      }
      default: break;
      }
   }

   for( auto delta : raw_delta_amounts )
      for( auto asset : delta.second )
         summary.delta_amounts[delta.first].emplace_back(asset.second, asset.first);
   if( tally_fees )
      for( auto fee : record.fees_paid )
         if( fee.second )
            summary.delta_amounts["Fee"].emplace_back(fee.second, fee.first);

   return cache[summary.id] = summary;
} FC_CAPTURE_AND_RETHROW( (transaction_bundle) ) }

fc::ecc::private_key light_wallet::create_one_time_key(const string& account_name, const string& key_id)
{
   FC_ASSERT(is_unlocked());
   //One time keys are hash(wif_active_key + " " + id) for some string ID.
   //A transaction OTK ID is its expiration time in seconds since epoch
   return fc::ecc::private_key::regenerate(fc::sha256::hash(
                                              fc::sha512::hash(
                                                 utilities::key_to_wif(active_key(account_name)) + " " + key_id)));
}

asset light_wallet::get_network_fee(const string& symbol)
{ try {
   auto symbol_asset = get_asset_record(symbol);
   if( symbol_asset.valid() && symbol_asset->id != 0 )
   {
      oprice median_feed = get_median_feed_price( symbol );
      if( median_feed )
         return _network_fee * *median_feed;
   }
   return _network_fee;
} FC_CAPTURE_AND_RETHROW( (symbol) ) }

} } // bts::light_wallet
