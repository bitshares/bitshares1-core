#include <bts/light_wallet/light_wallet.hpp>
#include <bts/blockchain/time.hpp>
#include <bts/blockchain/balance_operations.hpp>

#include <fc/network/resolve.hpp>
#include <fc/crypto/aes.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/io/json.hpp>

namespace bts { namespace light_wallet {
using std::string;

light_wallet::light_wallet()
{
}

light_wallet::~light_wallet()
{
}

void light_wallet::connect( const string& host, const string& user, const string& pass, uint16_t port )
{
   string last_error;
   auto resolved = fc::resolve( host, port ? port : BTS_LIGHT_WALLET_PORT );
   if( resolved.empty() ) last_error = "Unable to resolve host: " + host;

   for( auto item : resolved )
   {
      try {
         _rpc.connect_to( item );
         if( user != "any" && pass != "none" )
            _rpc.login( user, pass );
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
   return bool(_rpc.get_json_connection());
}

void light_wallet::disconnect()
{
   // TODO: note there is no clear way of closing the json connection
   auto json_con = _rpc.get_json_connection();
   //if( json_con ) json_con->close();
}

void light_wallet::open( const fc::path& wallet_json )
{
   _wallet_file = wallet_json;
   _data = fc::json::from_file<light_wallet_data>( wallet_json );
}

void light_wallet::save()
{
   fc::json::save_to_file(_data, _wallet_file, false);
}

void light_wallet::close()
{
   _wallet_file = fc::path();
   _private_key.reset();
   _data.reset();
}

void light_wallet::unlock( const string& password )
{ try {
   FC_ASSERT( is_open() );
   if( !_private_key )
   {
       auto pass_key = fc::sha512::hash( password );
       auto key_secret_data = fc::aes_decrypt( pass_key, _data->encrypted_private_key );
       _private_key = fc::raw::unpack<fc::ecc::private_key>( key_secret_data );
   }
} FC_CAPTURE_AND_RETHROW() }

bool light_wallet::is_unlocked()const { return _private_key.valid(); }
bool light_wallet::is_open()const { return _data.valid(); }

void light_wallet::change_password( const string& new_password )
{ try {
   FC_ASSERT( is_open() );
   FC_ASSERT( is_unlocked() );
   auto pass_key = fc::sha512::hash( new_password );
   _data->encrypted_private_key = fc::aes_decrypt( pass_key, fc::raw::pack( *_private_key ) );
   save();
} FC_CAPTURE_AND_RETHROW() }

void light_wallet::lock()
{
   _private_key.reset();
}

void light_wallet::create( const fc::path& wallet_json,
                           const string& account_name,
                           const string& password,
                           const string& brain_seed,
                           const string& salt )
{
   // initialize the wallet data
   _wallet_file = wallet_json;
   _data = light_wallet_data();

   // derive the brain wallet key
   fc::sha256::encoder enc;
   fc::raw::pack( enc, brain_seed );
   fc::raw::pack( enc, salt );
   _private_key = fc::ecc::private_key::regenerate( enc.result() );
   _data->user_account.owner_key = _private_key->get_public_key();

   // Don't actually store the owner key; it can be recovered via brain key and salt.
   // Locally, we'll only keep a deterministically derived child key so if that key is compromised,
   // the owner key remains safe.
   enc.reset();
   fc::raw::pack( enc, _private_key );
   // First active key has sequence number 0
   fc::raw::pack( enc, 0 );
   _private_key = fc::ecc::private_key::regenerate( enc.result() );
   _data->user_account.active_key_history[fc::time_point::now()] = _private_key->get_public_key();

   // set the password
   auto pass_key = fc::sha512::hash( password );
   _data->encrypted_private_key = fc::aes_encrypt( pass_key, fc::raw::pack( *_private_key ) );
   _data->user_account.name = account_name;
   _data->user_account.public_data = mutable_variant_object( "salt", salt );
   _data->user_account.meta_data = account_meta_info( public_account );
   save();
}

bool light_wallet::request_register_account()
{ try {
   FC_ASSERT( is_open() );
   try {
      return _rpc.request_register_account( _data->user_account );
   } catch (const fc::exception& e) {
      //If server gave back an account_already_registered, check if it's actually my account.
      if( e.code() == account_already_registered().code() &&
          _rpc.blockchain_get_account(_data->user_account.name)->active_key() != _data->user_account.active_key() )
         throw;
      return true;
   }
} FC_CAPTURE_AND_RETHROW( ) }

account_record& light_wallet::account()
{ try {
   FC_ASSERT( is_open() );
   return _data->user_account;
} FC_CAPTURE_AND_RETHROW( ) }

account_record& light_wallet::fetch_account()
{ try {
   FC_ASSERT( is_open() );
   auto account = _rpc.blockchain_get_account(_data->user_account.name);
   if( account )
   {
      if( account->active_key() != _data->user_account.active_key() )
         FC_THROW_EXCEPTION( account_already_registered,
                             "Attempted to fetch my account, but got one with an unknown active key.",
                             ("my_account", _data->user_account)("blockchain_account", account) );
      _data->user_account = *account;
      save();
   }

   return _data->user_account;
} FC_CAPTURE_AND_RETHROW( ) }

void light_wallet::transfer( double amount,
               const string& symbol,
               const string& to_account_name,
               const string& memo )
{ try {
   FC_ASSERT( is_unlocked() );

   auto symbol_asset = get_asset_record( symbol );
   FC_ASSERT( symbol_asset.valid() );

   auto to_account  = get_account_record( to_account_name );
   FC_ASSERT( to_account.valid() );

   asset fee = get_fee( symbol );
   (void)fee; // TODO use this

   // TODO include a fee payable to the light wallet service provider
   // TODO support multi-sig balances through service provider which
   // will perform N factor authentication

} FC_CAPTURE_AND_RETHROW( (amount)(symbol)(to_account_name)(memo) ) }

oprice light_wallet::get_median_feed_price( const string& symbol )
{
   if( is_open() )
   {
      auto cached_price_itr = _data->price_cache.find(symbol);
      if( cached_price_itr != _data->price_cache.end() )
      {
         if( cached_price_itr->second.second + fc::days(1) > fc::time_point::now() )
            return cached_price_itr->second.first;
         else
            _data->price_cache.erase( cached_price_itr );
      }
   }
   double price_ratio = _rpc.blockchain_median_feed_price( symbol );
   oprice opt;
   if( price_ratio != 0 )
   {
      auto base_rec = get_asset_record( BTS_BLOCKCHAIN_SYMBOL );
      auto quote_rec = get_asset_record( symbol );
      FC_ASSERT( base_rec && quote_rec );

      opt = price( price_ratio * (quote_rec->precision / base_rec->precision),
                   base_rec->id, quote_rec->id );
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
   FC_ASSERT( symbol_asset.valid() );
   if( symbol_asset->id != 0 )
   {
      oprice median_feed = get_median_feed_price( symbol );
      if( median_feed )
         return asset( 2*BTS_LIGHT_WALLET_DEFAULT_FEE, 0 ) * *median_feed;
   }
   return asset( BTS_LIGHT_WALLET_DEFAULT_FEE, 0 );
   } FC_CAPTURE_AND_RETHROW( (symbol) ) }

map<string, double> light_wallet::balance() const
{
   FC_ASSERT(is_open());

   map<string, double> balances = {{BTS_BLOCKCHAIN_SYMBOL, 0},{"USD", 0},{"GLD", 0}};
   for( auto balance : _data->balance_record_cache ) {
      asset_record record = _data->asset_record_cache.at(balance.second.asset_id());
      balances[record.symbol] += balance.second.balance / double(record.precision);
   }
   return balances;
}

vector<bts::wallet::transaction_ledger_entry> light_wallet::transactions(const string& symbol)
{
   FC_ASSERT( is_open() );

   vector<bts::wallet::transaction_ledger_entry> results;
   auto ids = _data->transaction_index[std::make_pair(_data->user_account.id, get_asset_record(symbol)->id)];
   std::for_each(ids.begin(), ids.end(), [this, &results](const transaction_id_type& id) {
      results.emplace_back(summarize(_data->transaction_record_cache[id]));
   });

   return results;
}


void light_wallet::sync_balance( bool resync_all )
{ try {
   FC_ASSERT( is_open() );

   if( resync_all )
   {
     _data->last_balance_sync_time = fc::time_point();
     _data->balance_record_cache.clear();
   }

   if( _data->last_balance_sync_time + fc::seconds(10) > fc::time_point::now() )
      return; // too fast

   fc::time_point sync_time = fc::time_point();

   auto new_balances = _rpc.blockchain_list_address_balances( string(address(_data->user_account.active_key())),
                                                              _data->last_balance_sync_time );
   std::vector<fc::variants> dirty_assets;

   for( auto item : new_balances )
   {
      _data->balance_record_cache[item.first] = item.second;
      dirty_assets.push_back(fc::variants(1, fc::variant(item.second.asset_id())));

      if( item.second.last_update > sync_time )
         sync_time = item.second.last_update;
   }

   auto asset_records = _rpc.batch("blockchain_get_asset", dirty_assets);
   for( int i = 0; i < asset_records.size(); ++i )
      _data->asset_record_cache[dirty_assets[i][0].as<asset_id_type>()] = asset_records[i].as<asset_record>();

   _data->last_balance_sync_time = sync_time;
   save();
} FC_CAPTURE_AND_RETHROW() }


void light_wallet::sync_transactions()
{
   FC_ASSERT( is_open() );

   uint32_t sync_block = _data->last_transaction_sync_block;
   auto new_trxs = _rpc.blockchain_list_address_transactions( string(address(_data->user_account.active_key())),
                                                              _data->last_transaction_sync_block );
   for( auto item : new_trxs )
   {
      fc::mutable_variant_object record("timestamp", item.second.first);
      record["trx"] = item.second.second;
      summarize(record);
      _data->transaction_record_cache[item.first] = record;
      if( item.second.second.chain_location.block_num > sync_block )
         sync_block = item.second.second.chain_location.block_num;
      for( const auto& delta : item.second.second.deltas )
         for( const auto& balance : delta.second )
            _data->transaction_index[std::make_pair(_data->user_account.id, balance.first)]
                  .insert(item.second.second.trx.id());
   }
   _data->last_transaction_sync_block = sync_block;
}

optional<asset_record> light_wallet::get_asset_record( const string& symbol )
{ try {
   if( is_open() )
   {
      for( auto item : _data->asset_record_cache )
         if( item.second.symbol == symbol )
            return item.second;
   }
   auto result = _rpc.blockchain_get_asset( symbol );
   if( result )
      _data->asset_record_cache[result->id] = *result;
   return result;
} FC_CAPTURE_AND_RETHROW( (symbol) ) }

optional<asset_record> light_wallet::get_asset_record(const asset_id_type& id)
{ try {
   if( is_open() )
   {
      for( auto item : _data->asset_record_cache )
         if( item.second.id == id )
            return item.second;
   }
   auto result = _rpc.blockchain_get_asset( fc::variant(id).as_string() );
   if( result )
      _data->asset_record_cache[result->id] = *result;
   return result;
   } FC_CAPTURE_AND_RETHROW( (id) ) }

oaccount_record light_wallet::get_account_record(const string& identifier)
{
   auto itr = _account_cache.find(identifier);
   if( itr != _account_cache.end() )
      return itr->second;
   wlog("Cache miss on account ${a}", ("a", identifier));

   auto account = _rpc.blockchain_get_account(identifier);
   if( account )
   {
      _account_cache[account->name] = *account;
      _account_cache[string(account->owner_key)] = *account;
      _account_cache[string(account->active_key())] = *account;
      _account_cache[string(account->owner_address())] = *account;
      _account_cache[string(account->active_address())] = *account;
   }
   return account;
}

bts::wallet::transaction_ledger_entry light_wallet::summarize(const fc::variant_object& transaction_bundle)
{ try {
   FC_ASSERT( is_open() );
   FC_ASSERT( is_unlocked() && _private_key );

   map<string, map<asset_id_type, share_type>> raw_delta_amounts;
   bts::wallet::transaction_ledger_entry summary;
   transaction_record record = transaction_bundle["trx"].as<transaction_record>();
   summary.timestamp = transaction_bundle["timestamp"].as<fc::time_point_sec>();
   summary.block_num = record.chain_location.block_num;
   summary.id = record.trx.id();

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
            if( condition.owner == _data->user_account.active_address() )
            {
               summary.delta_labels[i] = "Unknown>";
               if( condition.memo )
               {
                  omemo_status status = condition.decrypt_memo_data(*_private_key, true);
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
               summary.delta_labels[i] += _data->user_account.name;
            } else {
               summary.delta_labels[i] = string(condition.owner);
               auto account = get_account_record(string(condition.owner));
               if( account ) summary.delta_labels[i] = account->name;
            }

            raw_delta_amounts[summary.delta_labels[i]][asset_id] += record.deltas[i][asset_id];
         }
         break;
      }
      case withdraw_op_type: {
         withdraw_operation withdrawal = op.as<withdraw_operation>();
         if( _data->balance_record_cache.count(withdrawal.balance_id) )
         {
            balance_record balance = _data->balance_record_cache[withdrawal.balance_id];
            if( balance.owners().count(_data->user_account.active_key()) )
               summary.delta_labels[i] = _data->user_account.name;
         }
         break;
      }
      default: break;
      }
   }

   for( auto delta : raw_delta_amounts )
      for( auto asset : delta.second )
         summary.delta_amounts[delta.first].emplace_back(asset.second, asset.first);
   for( auto fee : record.balance )
      summary.delta_amounts["Fee"].emplace_back(fee.second, fee.first);

   return summary;
} FC_CAPTURE_AND_RETHROW( (transaction_bundle) ) }

} } // bts::light_wallet
