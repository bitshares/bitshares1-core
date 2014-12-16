#include <bts/light_wallet/light_wallet.hpp>
#include <bts/blockchain/time.hpp>

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

void light_wallet::connect( const string& host, const string& user, const string& pass, uint16_t port  )
{
   string last_error;
   auto resolved = fc::resolve( host, port ? port : BTS_LIGHT_WALLET_PORT );
   if( resolved.empty() ) last_error = "Unable to resolve host: " + host;

   for( auto item : resolved )
   {
      try {
         _rpc.connect_to( item );
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

   // set the password
   auto pass_key = fc::sha512::hash( password );
   _data->encrypted_private_key = fc::aes_decrypt( pass_key, fc::raw::pack( *_private_key ) );
   _data->user_account.owner_key = _private_key->get_public_key();
   _data->user_account.public_data = mutable_variant_object( "salt", salt );
   _data->user_account.meta_data = account_meta_info( public_account );
   // TODO: set active key to be the first child of the owner key, the light wallet
   // should never store the owner master key.  It should only be recoverable with
   // the brain_seed + salt
   save();
}

void light_wallet::request_register_account( const string& account_name )
{ try {
   FC_ASSERT( is_open() );
   _data->user_account.name = fc::to_lower(account_name);
   _rpc.request_register_account( _data->user_account );
} FC_CAPTURE_AND_RETHROW( (account_name) ) }

void light_wallet::transfer( double amount,
               const string& symbol,
               const string& to_account_name,
               const string& memo )
{ try {
   FC_ASSERT( is_unlocked() );

   auto symbol_asset = get_asset_record( symbol );
   FC_ASSERT( symbol_asset.valid() );

   auto to_account  = _rpc.blockchain_get_account( to_account_name );
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

   auto sync_time = bts::blockchain::now();

   auto new_balances = _rpc.blockchain_list_address_balances( string(address(_data->user_account.active_key())),
                                                              _data->last_balance_sync_time );
   for( auto item : new_balances )
      _data->balance_record_cache[item.first] = item.second;

   _data->last_balance_sync_time = sync_time;
} FC_CAPTURE_AND_RETHROW() }


void light_wallet::sync_transactions()
{
   FC_ASSERT( is_open() );
   if( _data->last_transaction_sync_time + fc::seconds(10) > fc::time_point::now() )
      return; // too fast

   auto sync_time = bts::blockchain::now();
   auto new_trxs = _rpc.blockchain_list_address_transactions( string(address(_data->user_account.active_key())),
                                                              _data->last_transaction_sync_time );
   for( auto item : new_trxs )
      _data->transaction_record_cache[item.first] = item.second;
   _data->last_transaction_sync_time = sync_time;
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



} } // bts::wallet
