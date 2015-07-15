#include <bts/client/client.hpp>
#include <bts/client/client_impl.hpp>
#include <bts/utilities/key_conversion.hpp>
#include <bts/utilities/words.hpp>
#include <bts/wallet/config.hpp>
#include <bts/wallet/exceptions.hpp>

#include <fc/crypto/aes.hpp>
#include <fc/network/http/connection.hpp>
#include <fc/network/resolve.hpp>
#include <fc/network/url.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/thread/non_preemptable_scope_check.hpp>

namespace bts { namespace client { namespace detail {

void detail::client_impl::wallet_open(const string& wallet_name)
{
  _wallet->open(fc::trim(wallet_name));
  reschedule_delegate_loop();
}

fc::optional<variant> detail::client_impl::wallet_get_setting(const string& name)
{
  return _wallet->get_setting( name );
}

void detail::client_impl::wallet_set_setting(const string& name, const variant& value)
{
  _wallet->set_setting( name, value );
}


void detail::client_impl::wallet_create( const string& wallet_name, const string& new_passphrase, const string& brain_key,
                                         const string& new_passphrase_verify )
{
    if( !new_passphrase_verify.empty() )
        FC_ASSERT( new_passphrase_verify == new_passphrase );

    string trimmed_name = fc::trim( wallet_name );
    if( trimmed_name.size() == 0 )
        FC_CAPTURE_AND_THROW( fc::invalid_arg_exception, (trimmed_name) );

    if( new_passphrase.size() < BTS_WALLET_MIN_PASSWORD_LENGTH )
        FC_CAPTURE_AND_THROW( password_too_short );

    if( brain_key.size() && brain_key.size() < BTS_WALLET_MIN_BRAINKEY_LENGTH )
        FC_CAPTURE_AND_THROW( brain_key_too_short );

    _wallet->create( trimmed_name, new_passphrase, brain_key );

    reschedule_delegate_loop();
}

void detail::client_impl::wallet_close()
{
    if( !_wallet ) return;
    _wallet->close();
    reschedule_delegate_loop();
}

void detail::client_impl::wallet_backup_create( const fc::path& json_filename )const
{
    _wallet->export_to_json( json_filename );
}

void detail::client_impl::wallet_backup_restore( const fc::path& json_filename,
                                                 const string& wallet_name,
                                                 const string& imported_wallet_passphrase )
{
    _wallet->create_from_json( json_filename, wallet_name, imported_wallet_passphrase );
    reschedule_delegate_loop();
}

void detail::client_impl::wallet_export_keys( const fc::path& json_filename )const
{
    _wallet->export_keys( json_filename );
}

// This should be able to get an encrypted private key or WIF key out of any reasonable JSON object
void read_keys( const fc::variant& vo, vector<private_key_type>& keys, const string& password )
{
    try
    {
      const auto wif_key = vo.as_string();
      const auto key = bts::utilities::wif_to_key( wif_key );
      if( key.valid() ) keys.push_back(*key);
    }
    catch( ... )
    {
        //ilog("@n I couldn't parse that as a wif key: ${vo}", ("vo", vo));
    }

    try
    {
        const auto key_bytes = vo.as<vector<char>>();
        const auto password_bytes = fc::sha512::hash( password.c_str(), password.size() );
        const auto key_plain_text = fc::aes_decrypt( password_bytes, key_bytes );
        keys.push_back( fc::raw::unpack<private_key_type>( key_plain_text ) );
    }
    catch( ... )
    {
        //ilog("@n I couldn't parse that as a byte array: ${vo}", ("vo", vo));

    }

    try
    {
        const auto obj = vo.get_object();
        for( const auto& kv : obj )
        {
            read_keys( kv.value(), keys, password );
        }
    }
    catch( const bts::wallet::invalid_password& e )
    {
        throw;
    }
    catch( ... )
    {
        //ilog("@n I couldn't parse that as an object: ${o}", ("o", vo));
    }

    try
    {
        const auto arr = vo.as<vector<variant>>();
        for( const auto& obj : arr )
        {
            read_keys( obj, keys, password );
        }
    }
    catch( const bts::wallet::invalid_password& e )
    {
        throw;
    }
    catch( ... )
    {
        //ilog("@n I couldn't parse that as an array: ${o}", ("o", vo));
    }

    //ilog("@n I couldn't parse that as anything!: ${o}", ("o", vo));
}

uint32_t detail::client_impl::wallet_import_keys_from_json( const fc::path& json_filename,
                                                            const string& imported_wallet_passphrase,
                                                            const string& account )
{ try {
    FC_ASSERT( fc::exists( json_filename ) );
    FC_ASSERT( _wallet->is_open() );
    FC_ASSERT( _wallet->is_unlocked() );
    _wallet->get_account( account );

    const auto object = fc::json::from_file<fc::variant>( json_filename );
    vector<private_key_type> keys;
    read_keys( object, keys, imported_wallet_passphrase );

    uint32_t count = 0;
    for( const auto& key : keys )
        count += _wallet->friendly_import_private_key( key, account );

    _wallet->auto_backup( "json_key_import" );
    ulog( "Successfully imported ${n} new private keys into account ${name}", ("n",count)("name",account) );

    _wallet->start_scan( 0, 1 );

    return count;
} FC_CAPTURE_AND_RETHROW( (json_filename) ) }

bool detail::client_impl::wallet_set_automatic_backups( bool enabled )
{
    _wallet->set_automatic_backups( enabled );
    return _wallet->get_automatic_backups();
}

uint32_t detail::client_impl::wallet_set_transaction_expiration_time( uint32_t secs )
{
    _wallet->set_transaction_expiration( secs );
    return _wallet->get_transaction_expiration();
}

void detail::client_impl::wallet_lock()
{
  _wallet->lock();
  reschedule_delegate_loop();
}

void detail::client_impl::wallet_unlock( uint32_t timeout, const string& password )
{
  _wallet->unlock(password, timeout);
  reschedule_delegate_loop();
  if( _config.wallet_callback_url.size() > 0 )
  {
     _http_callback_signal_connection =
        _wallet->wallet_claimed_transaction.connect(
            [=]( ledger_entry e ) { this->wallet_http_callback( _config.wallet_callback_url, e ); } );
  }
}

void detail::client_impl::wallet_http_callback( const string& url, const ledger_entry& e )
{
   fc::async( [=]()
              {
                  fc::url u(url);
                  if( u.host() )
                  {
                     auto endpoints = fc::resolve( *u.host(), u.port() ? *u.port() : 80 );
                     for( auto ep : endpoints )
                     {
                        fc::http::connection con;
                        con.connect_to( ep );
                        auto response = con.request( "POST", url, fc::json::to_string(e) );
                        if( response.status == fc::http::reply::OK )
                           return;
                     }
                  }
              }
            );
}


void detail::client_impl::wallet_change_passphrase( const string& new_passphrase, const string& new_passphrase_verify )
{
    if( !new_passphrase_verify.empty() )
        FC_ASSERT( new_passphrase_verify == new_passphrase );

    _wallet->auto_backup( "passphrase_change" );
    _wallet->change_passphrase( new_passphrase );
    reschedule_delegate_loop();
}

map<transaction_id_type, fc::exception> detail::client_impl::wallet_get_pending_transaction_errors( const string& filename )const
{
  const auto& errors = _wallet->get_pending_transaction_errors();
  if( filename != "" )
  {
      FC_ASSERT( !fc::exists( filename ) );
      std::ofstream out( filename.c_str() );
      out << fc::json::to_pretty_string( errors );
  }
  return errors;
}

wallet_transaction_record detail::client_impl::wallet_publish_slate( const string& publishing_account_name,
                                                                     const string& paying_account_name )
{
   auto record = _wallet->publish_slate( publishing_account_name, paying_account_name, true );
   _wallet->cache_transaction( record );
   network_broadcast_transaction( record.trx );
   return record;
}

wallet_transaction_record detail::client_impl::wallet_publish_version( const string& publishing_account_name,
                                                                       const string& paying_account_name )
{
   auto record = _wallet->publish_version( publishing_account_name, paying_account_name, true );
   _wallet->cache_transaction( record );
   network_broadcast_transaction( record.trx );
   return record;
}

wallet_transaction_record detail::client_impl::wallet_collect_genesis_balances( const string& account_name )
{
    const auto filter = []( const balance_record& record ) -> bool
    {
        return record.condition.type == withdraw_signature_type && record.snapshot_info.valid();
    };
    auto record = _wallet->collect_account_balances( account_name, filter, "collect genesis", true );
    _wallet->cache_transaction( record );
    network_broadcast_transaction( record.trx );
    return record;
}

wallet_transaction_record detail::client_impl::wallet_collect_vested_balances( const string& account_name )
{
    const auto filter = []( const balance_record& record ) -> bool
    {
        return record.condition.type == withdraw_vesting_type;
    };
    auto record = _wallet->collect_account_balances( account_name, filter, "collect vested", true );
    _wallet->cache_transaction( record );
    network_broadcast_transaction( record.trx );
    return record;
}

wallet_transaction_record detail::client_impl::wallet_delegate_update_signing_key( const string& authorizing_account_name,
                                                                                   const string& delegate_name,
                                                                                   const public_key_type& signing_key )
{
   auto record = _wallet->update_signing_key( authorizing_account_name, delegate_name, signing_key, true );
   _wallet->cache_transaction( record );
   network_broadcast_transaction( record.trx );
   return record;
}

int32_t detail::client_impl::wallet_recover_accounts( int32_t accounts_to_recover, int32_t maximum_number_of_attempts )
{
  _wallet->auto_backup( "before_account_recovery" );
  return _wallet->recover_accounts( accounts_to_recover, maximum_number_of_attempts );
}

wallet_transaction_record detail::client_impl::wallet_recover_titan_deposit_info( const string& transaction_id_prefix,
                                                                                  const string& recipient_account )
{
    return _wallet->recover_transaction( transaction_id_prefix, recipient_account );
}

optional<variant_object> detail::client_impl::wallet_verify_titan_deposit( const string& transaction_id_prefix )
{
    return _wallet->verify_titan_deposit( transaction_id_prefix );
}

wallet_transaction_record detail::client_impl::wallet_transfer(
        const string& amount_to_transfer,
        const string& asset_symbol,
        const string& from_account_name,
        const string& recipient,
        const string& memo_message,
        const vote_strategy& strategy )
{ try {
    const asset amount = _chain_db->to_ugly_asset( amount_to_transfer, asset_symbol );

    auto record = _wallet->transfer( amount, from_account_name, recipient, memo_message, strategy, true );
    _wallet->cache_transaction( record );
    network_broadcast_transaction( record.trx );

    return record;
} FC_CAPTURE_AND_RETHROW( (amount_to_transfer)(asset_symbol)(from_account_name)(recipient)(memo_message)(strategy) ) }

wallet_transaction_record detail::client_impl::wallet_burn(
        const string& amount_to_transfer,
        const string& asset_symbol,
        const string& from_account_name,
        const string& for_or_against,
        const string& to_account_name,
        const string& public_message,
        bool anonymous )
{
    const asset amount = _chain_db->to_ugly_asset( amount_to_transfer, asset_symbol );
    auto record = _wallet->burn_asset( amount,
                                       from_account_name, for_or_against, to_account_name,
                                       public_message, anonymous, true );
    _wallet->cache_transaction( record );
    network_broadcast_transaction( record.trx );
    return record;
}

string detail::client_impl::wallet_address_create( const string& account_name,
                                                    const string& label,
                                                    int legacy_network_byte )
{ try {
    auto pubkey = _wallet->get_new_public_key( account_name );
    if( legacy_network_byte == -1 )
        return string( address(pubkey) );
    else if (legacy_network_byte == 0 || legacy_network_byte == 56)
        return string( pts_address( pubkey, true, legacy_network_byte ) );
    else
        FC_ASSERT(false, "Unsupported network byte");
} FC_CAPTURE_AND_RETHROW( (account_name)(label)(legacy_network_byte) ) }

balance_id_type detail::client_impl::wallet_multisig_get_balance_id(
                                        const string& asset_symbol,
                                        uint32_t m,
                                        const vector<address>& addresses )const
{
    auto id = _chain_db->get_asset_id( asset_symbol );
    return balance_record::get_multisig_balance_id( id, m, addresses );
}

wallet_transaction_record detail::client_impl::wallet_multisig_deposit(
                                                    const string& amount,
                                                    const string& symbol,
                                                    const string& from_name,
                                                    uint32_t m,
                                                    const vector<address>& addresses,
                                                    const vote_strategy& strategy )
{
    asset ugly_asset = _chain_db->to_ugly_asset(amount, symbol);
    auto builder = _wallet->create_transaction_builder();
    builder->deposit_asset_to_multisig( ugly_asset, from_name, m, addresses );
    auto record = builder->finalize( true, strategy ).sign();
    _wallet->cache_transaction( record );
    network_broadcast_transaction( record.trx );
    return record;
}

transaction_builder detail::client_impl::wallet_withdraw_from_address(
                                                    const string& amount,
                                                    const string& symbol,
                                                    const address& from_address,
                                                    const string& to,
                                                    const vote_strategy& strategy,
                                                    bool sign,
                                                    const string& builder_path )
{
    address to_address;
    try {
        auto acct = _wallet->get_account( to );
        to_address = acct.owner_address();
    } catch (...) {
        to_address = address( to );
    }
    asset ugly_asset = _chain_db->to_ugly_asset(amount, symbol);
    auto builder = _wallet->create_transaction_builder();
    auto fee = _wallet->get_transaction_fee();
    builder->withdraw_from_balance( from_address, ugly_asset.amount + fee.amount );
    builder->deposit_to_balance( to_address, ugly_asset );
    builder->finalize( false, strategy );
    if( sign )
    {
        builder->sign();
        network_broadcast_transaction( builder->transaction_record.trx );
    }
    _wallet->write_latest_builder( *builder, builder_path );
    return *builder;
}

transaction_builder detail::client_impl::wallet_withdraw_from_legacy_address(
                                                    const string& amount,
                                                    const string& symbol,
                                                    const pts_address& from_address,
                                                    const string& to,
                                                    const vote_strategy& strategy,
                                                    bool sign,
                                                    const string& builder_path )const
{
    address to_address;
    try {
        auto acct = _wallet->get_account( to );
        to_address = acct.owner_address();
    } catch (...) {
        to_address = address( to );
    }
    asset ugly_asset = _chain_db->to_ugly_asset(amount, symbol);
    auto builder = _wallet->create_transaction_builder();
    auto fee = _wallet->get_transaction_fee();
    builder->withdraw_from_balance( from_address, ugly_asset.amount + fee.amount );
    builder->deposit_to_balance( to_address, ugly_asset );
    builder->finalize( false, strategy );
    if( sign )
        builder->sign();
    _wallet->write_latest_builder( *builder, builder_path );
    return *builder;
}

transaction_builder detail::client_impl::wallet_multisig_withdraw_start(
                                                    const string& amount,
                                                    const string& symbol,
                                                    const balance_id_type& from,
                                                    const address& to_address,
                                                    const vote_strategy& strategy,
                                                    const string& builder_path )const
{
    asset ugly_asset = _chain_db->to_ugly_asset(amount, symbol);
    auto builder = _wallet->create_transaction_builder();
    auto fee = _wallet->get_transaction_fee();
    builder->withdraw_from_balance( from, ugly_asset.amount + fee.amount );
    builder->deposit_to_balance( to_address, ugly_asset );
    _wallet->write_latest_builder( *builder, builder_path );
    return *builder;
}

transaction_builder detail::client_impl::wallet_builder_add_signature(
                                            const transaction_builder& builder,
                                            bool broadcast )
{ try {
    auto new_builder = _wallet->create_transaction_builder( builder );
    if( new_builder->transaction_record.trx.signatures.empty() )
        new_builder->finalize( false );
    new_builder->sign();
    if( broadcast )
    {
        try {
            network_broadcast_transaction( new_builder->transaction_record.trx );
        }
        catch(...) {
            ulog("I tried to broadcast the transaction but it was not valid.");
        }
    }
    _wallet->write_latest_builder( *new_builder, "" );
    return *new_builder;
} FC_CAPTURE_AND_RETHROW( (builder)(broadcast) ) }

transaction_builder detail::client_impl::wallet_builder_file_add_signature(
                                            const string& builder_path,
                                            bool broadcast )
{ try {
    auto new_builder = _wallet->create_transaction_builder_from_file( builder_path );
    if( new_builder->transaction_record.trx.signatures.empty() )
        new_builder->finalize( false );
    new_builder->sign();
    if( broadcast )
    {
        try {
            network_broadcast_transaction( new_builder->transaction_record.trx );
        }
        catch(...) {
            ulog("I tried to broadcast the transaction but it was not valid.");
        }
    }
    _wallet->write_latest_builder( *new_builder, builder_path );
    _wallet->write_latest_builder( *new_builder, "" ); // always write to "latest"
    return *new_builder;
} FC_CAPTURE_AND_RETHROW( (broadcast)(builder_path) ) }

wallet_transaction_record detail::client_impl::wallet_release_escrow( const string& paying_account_name,
                                                                      const address& escrow_balance_id,
                                                                      const string& released_by,
                                                                      const string& amount_to_sender_string,
                                                                      const string& amount_to_receiver_string )
{
    auto payer = _wallet->get_account(paying_account_name);
    auto balance_rec = _chain_db->get_balance_record( escrow_balance_id );
    FC_ASSERT( balance_rec.valid() );
    FC_ASSERT( balance_rec->condition.type == withdraw_escrow_type );
    FC_ASSERT( released_by == "sender" ||
               released_by == "receiver" ||
               released_by == "agent" );

    auto asset_rec = _chain_db->get_asset_record( balance_rec->asset_id() );
    FC_ASSERT( asset_rec.valid() );

    const asset amount_to_sender = _chain_db->to_ugly_asset( amount_to_sender_string, asset_rec->symbol );
    const asset amount_to_receiver = _chain_db->to_ugly_asset( amount_to_receiver_string, asset_rec->symbol );

    auto escrow_cond = balance_rec->condition.as<withdraw_with_escrow>();
    address release_by_address;

    if( released_by == "sender" ) release_by_address = escrow_cond.sender;
    if( released_by == "receiver" ) release_by_address = escrow_cond.receiver;
    if( released_by == "agent" ) release_by_address = escrow_cond.escrow;

    transaction_builder_ptr builder = _wallet->create_transaction_builder();
    auto record = builder->release_escrow( payer, escrow_balance_id, release_by_address, amount_to_sender, amount_to_receiver )
  //  TODO: restore this function       .finalize()
                                        .sign();

    _wallet->cache_transaction( record );
    network_broadcast_transaction( record.trx );
    return record;
}

wallet_transaction_record detail::client_impl::wallet_transfer_from_with_escrow(
        const string& amount_to_transfer,
        const string& asset_symbol,
        const string& paying_account_name,
        const string& from_account_name,
        const string& to_account_name,
        const string& escrow_account_name,
        const digest_type&   agreement,
        const string& memo_message,
        const vote_strategy& strategy )
{
    asset amount = _chain_db->to_ugly_asset( amount_to_transfer, asset_symbol );
    auto sender = _wallet->get_account(from_account_name);
    auto payer = _wallet->get_account(paying_account_name);
    auto recipient = _wallet->get_account(to_account_name);
    auto escrow_account = _wallet->get_account(escrow_account_name);
    transaction_builder_ptr builder = _wallet->create_transaction_builder();

    auto record = builder->deposit_asset_with_escrow(payer, recipient, escrow_account, agreement,
                                                     amount, memo_message, sender.owner_key)
                          .finalize( true, strategy )
                          .sign();

    _wallet->cache_transaction( record );
    network_broadcast_transaction( record.trx );

    return record;
}

wallet_transaction_record detail::client_impl::wallet_mia_create(
        const string& payer_account,
        const string& symbol,
        const string& name,
        const string& description,
        const string& max_divisibility
        )
{ try {
    const uint64_t precision = asset_record::share_string_to_precision_unsafe( max_divisibility );
    const share_type max_supply = BTS_BLOCKCHAIN_MAX_SHARES;

    auto record = _wallet->asset_register( payer_account, symbol, name, description, max_supply, precision, false, true );
    _wallet->cache_transaction( record );
    network_broadcast_transaction( record.trx );

    return record;
} FC_CAPTURE_AND_RETHROW( (payer_account)(symbol)(name)(description)(max_divisibility) ) }

wallet_transaction_record detail::client_impl::wallet_uia_create(
        const string& issuer_account,
        const string& symbol,
        const string& name,
        const string& description,
        const string& max_supply_with_trailing_decimals
        )
{ try {
    const uint64_t precision = asset_record::share_string_to_precision_unsafe( max_supply_with_trailing_decimals );
    const share_type max_supply = asset_record::share_string_to_satoshi( max_supply_with_trailing_decimals, precision );

    auto record = _wallet->asset_register( issuer_account, symbol, name, description, max_supply, precision, true, true );
    _wallet->cache_transaction( record );
    network_broadcast_transaction( record.trx );

    return record;
} FC_CAPTURE_AND_RETHROW( (issuer_account)(symbol)(name)(description)(max_supply_with_trailing_decimals) ) }

wallet_transaction_record detail::client_impl::wallet_uia_issue(
        const string& asset_amount,
        const string& asset_symbol,
        const string& recipient,
        const string& memo_message
        )
{ try {
    const asset amount = _chain_db->to_ugly_asset( asset_amount, asset_symbol );

    auto record = _wallet->uia_issue_or_collect_fees( true, amount, recipient, memo_message, true );
    _wallet->cache_transaction( record );
    network_broadcast_transaction( record.trx );

    return record;
} FC_CAPTURE_AND_RETHROW( (asset_amount)(asset_symbol)(recipient)(memo_message) ) }

wallet_transaction_record detail::client_impl::wallet_uia_issue_to_addresses(
        const string& symbol,
        const map<string, share_type>& addresses
        )
{
  auto record = _wallet->uia_issue_to_many( symbol, addresses );
  _wallet->cache_transaction( record );
  network_broadcast_transaction( record.trx );
  return record;
}

wallet_transaction_record detail::client_impl::wallet_uia_collect_fees(
        const string& asset_symbol,
        const string& recipient,
        const string& memo_message
        )
{ try {
    const oasset_record asset_record = _chain_db->get_asset_record( asset_symbol );
    FC_ASSERT( asset_record.valid() );

    const asset amount = asset( asset_record->collected_fees, asset_record->id );

    auto record = _wallet->uia_issue_or_collect_fees( false, amount, recipient, memo_message, true );
    _wallet->cache_transaction( record );
    network_broadcast_transaction( record.trx );

    return record;
} FC_CAPTURE_AND_RETHROW( (asset_symbol)(recipient)(memo_message) ) }

wallet_transaction_record detail::client_impl::wallet_uia_update_description(
        const string& paying_account,
        const string& asset_symbol,
        const string& name,
        const string& description,
        const variant& public_data
        )
{ try {
    const oasset_record asset_record = _chain_db->get_asset_record( asset_symbol );
    FC_ASSERT( asset_record.valid() );

    asset_update_properties_operation update_op;
    update_op.asset_id = asset_record->id;

    if( !name.empty() ) update_op.name = name;
    if( !description.empty() ) update_op.description = description;
    if( !public_data.is_null() ) update_op.public_data = public_data;

    auto record = _wallet->uia_update_properties( paying_account, asset_symbol, update_op, true );
    _wallet->cache_transaction( record );
    network_broadcast_transaction( record.trx );
    return record;
} FC_CAPTURE_AND_RETHROW( (paying_account)(asset_symbol)(name)(description)(public_data) ) }

wallet_transaction_record detail::client_impl::wallet_uia_update_supply(
        const string& paying_account,
        const string& asset_symbol,
        const string& max_supply_with_trailing_decimals
        )
{ try {
    FC_ASSERT( !max_supply_with_trailing_decimals.empty() );

    const uint64_t precision = asset_record::share_string_to_precision_unsafe( max_supply_with_trailing_decimals );
    const share_type max_supply = asset_record::share_string_to_satoshi( max_supply_with_trailing_decimals, precision );

    const oasset_record asset_record = _chain_db->get_asset_record( asset_symbol );
    FC_ASSERT( asset_record.valid() );

    asset_update_properties_operation update_op;
    update_op.asset_id = asset_record->id;

    if( precision != asset_record->precision ) update_op.precision = precision;
    if( max_supply != asset_record->max_supply ) update_op.max_supply = max_supply;

    auto record = _wallet->uia_update_properties( paying_account, asset_symbol, update_op, true );
    _wallet->cache_transaction( record );
    network_broadcast_transaction( record.trx );
    return record;
} FC_CAPTURE_AND_RETHROW( (paying_account)(asset_symbol)(max_supply_with_trailing_decimals) ) }

wallet_transaction_record detail::client_impl::wallet_uia_update_fees(
        const string& paying_account,
        const string& asset_symbol,
        const string& withdrawal_fee_string,
        const string& market_fee_rate_string
        )
{ try {
    const oasset_record asset_record = _chain_db->get_asset_record( asset_symbol );
    FC_ASSERT( asset_record.valid() );

    asset_update_properties_operation update_op;
    update_op.asset_id = asset_record->id;

    if( !withdrawal_fee_string.empty() )
    {
        const share_type withdrawal_fee = asset_record::share_string_to_satoshi( withdrawal_fee_string, asset_record->precision );
        if( withdrawal_fee != asset_record->withdrawal_fee ) update_op.withdrawal_fee = withdrawal_fee;
    }

    if( !market_fee_rate_string.empty() )
    {
        const uint16_t market_fee_rate = uint16_t( asset_record::share_string_to_satoshi( market_fee_rate_string, 100 ) );
        if( market_fee_rate != asset_record->market_fee_rate ) update_op.market_fee_rate = market_fee_rate;
    }

    auto record = _wallet->uia_update_properties( paying_account, asset_symbol, update_op, true );
    _wallet->cache_transaction( record );
    network_broadcast_transaction( record.trx );
    return record;
} FC_CAPTURE_AND_RETHROW( (paying_account)(asset_symbol)(withdrawal_fee_string)(market_fee_rate_string) ) }

wallet_transaction_record detail::client_impl::wallet_uia_update_active_flags(
        const string& paying_account,
        const string& asset_symbol,
        const asset_record::flag_enum& flag,
        const bool enable_instead_of_disable
        )
{ try {
    auto record = _wallet->uia_update_permission_or_flag( paying_account, asset_symbol, flag, enable_instead_of_disable, false, true );
    _wallet->cache_transaction( record );
    network_broadcast_transaction( record.trx );
    return record;
} FC_CAPTURE_AND_RETHROW( (paying_account)(asset_symbol)(flag)(enable_instead_of_disable) ) }

wallet_transaction_record detail::client_impl::wallet_uia_update_authority_permissions(
        const string& paying_account,
        const string& asset_symbol,
        const asset_record::flag_enum& permission,
        const bool add_instead_of_remove
        )
{ try {
    auto record = _wallet->uia_update_permission_or_flag( paying_account, asset_symbol, permission, add_instead_of_remove, true, true );
    _wallet->cache_transaction( record );
    network_broadcast_transaction( record.trx );
    return record;
} FC_CAPTURE_AND_RETHROW( (paying_account)(asset_symbol)(permission)(add_instead_of_remove) ) }

wallet_transaction_record detail::client_impl::wallet_uia_update_whitelist(
        const string& paying_account,
        const string& asset_symbol,
        const string& account_name,
        const bool add_to_whitelist
        )
{ try {
    auto record = _wallet->uia_update_whitelist( paying_account, asset_symbol, account_name, add_to_whitelist, true );
    _wallet->cache_transaction( record );
    network_broadcast_transaction( record.trx );
    return record;
} FC_CAPTURE_AND_RETHROW( (paying_account)(asset_symbol)(account_name)(add_to_whitelist) ) }

wallet_transaction_record detail::client_impl::wallet_uia_retract_balance(
        const balance_id_type& balance_id,
        const string& account_name
        )
{ try {
    auto record = _wallet->uia_retract_balance( balance_id, account_name, true );
    _wallet->cache_transaction( record );
    network_broadcast_transaction( record.trx );
    return record;
} FC_CAPTURE_AND_RETHROW( (balance_id)(account_name) ) }

vector<string> detail::client_impl::wallet_list() const
{
  return _wallet->list();
}

vector<wallet_account_record> detail::client_impl::wallet_list_accounts() const
{
  return _wallet->list_accounts();
}

void detail::client_impl::wallet_account_rename( const string& current_account_name, const string& new_account_name )
{
  _wallet->rename_account(current_account_name, new_account_name);
  _wallet->auto_backup( "account_rename" );
}

owallet_account_record detail::client_impl::wallet_get_account( const string& account )const
{ try {
    return _wallet->lookup_account( account );
} FC_CAPTURE_AND_RETHROW( (account) ) }

vector<pretty_transaction> detail::client_impl::wallet_account_transaction_history( const string& account_name,
                                                                                    const string& asset_symbol,
                                                                                    int32_t limit,
                                                                                    uint32_t start_block_num,
                                                                                    uint32_t end_block_num )const
{ try {
  const auto history = _wallet->get_pretty_transaction_history( account_name, start_block_num, end_block_num, asset_symbol );
  if( limit == 0 || abs( limit ) >= history.size() )
  {
      return history;
  }
  else if( limit > 0 )
  {
      return vector<pretty_transaction>( history.begin(), history.begin() + limit );
  }
  else
  {
      return vector<pretty_transaction>( history.end() - abs( limit ), history.end() );
  }
} FC_RETHROW_EXCEPTIONS( warn, "") }

account_balance_summary_type detail::client_impl::wallet_account_historic_balance( const time_point& time,
                                                                                     const string& account )const
{ try {
    fc::time_point_sec target(time);
    return _wallet->compute_historic_balance( account,
                                              _self->get_chain()->find_block_num( target ) );
} FC_RETHROW_EXCEPTIONS( warn, "") }

void detail::client_impl::wallet_remove_transaction( const string& transaction_id )
{ try {
   _wallet->remove_transaction_record( transaction_id );
} FC_RETHROW_EXCEPTIONS( warn, "", ("transaction_id",transaction_id) ) }

void detail::client_impl::wallet_rebroadcast_transaction( const string& transaction_id )
{ try {
   const auto records = _wallet->get_transactions( transaction_id );
   for( const auto& record : records )
   {
       if( record.is_virtual ) continue;
       network_broadcast_transaction( record.trx );
       std::cout << "Rebroadcasted transaction: " << string( record.trx.id() ) << "\n";
   }
} FC_RETHROW_EXCEPTIONS( warn, "", ("transaction_id",transaction_id) ) }

uint32_t detail::client_impl::wallet_import_bitcoin(
        const fc::path& filename,
        const string& passphrase,
        const string& account_name
        )
{
  try
  {
      const auto count = _wallet->import_bitcoin_wallet(filename, "", account_name);
      _wallet->auto_backup( "bitcoin_import" );
      return count;
  }
  catch ( const fc::canceled_exception& )
  {
      throw;
  }
  catch( const fc::exception& e )
  {
      wlog( "import_bitcoin_wallet failed with empty password: ${e}", ("e",e.to_detail_string() ) );
  }

  const auto count = _wallet->import_bitcoin_wallet(filename, passphrase, account_name);
  _wallet->auto_backup( "bitcoin_import" );
  return count;
}

uint32_t detail::client_impl::wallet_import_electrum(
        const fc::path& filename,
        const string& passphrase,
        const string& account_name
        )
{
  const auto count = _wallet->import_electrum_wallet(filename, passphrase, account_name);
  _wallet->auto_backup( "electrum_import" );
  return count;
}

void detail::client_impl::wallet_import_keyhotee(const string& firstname,
                                                 const string& middlename,
                                                 const string& lastname,
                                                 const string& brainkey,
                                                 const string& keyhoteeid)
{
   _wallet->import_keyhotee(firstname, middlename, lastname, brainkey, keyhoteeid);
  _wallet->auto_backup( "keyhotee_import" );
}

string detail::client_impl::wallet_import_private_key( const string& wif_key_to_import,
                                                       const string& account_name,
                                                       bool create_account,
                                                       bool wallet_rescan_blockchain )
{
  optional<string> name;
  if( !account_name.empty() )
      name = account_name;

  const public_key_type new_public_key = _wallet->import_wif_private_key( wif_key_to_import, name, create_account );

  if( wallet_rescan_blockchain )
      _wallet->start_scan( 0, -1 );
  else
      _wallet->start_scan( 0, 1 );

  const owallet_account_record account_record = _wallet->get_account_for_address( address( new_public_key ) );
  FC_ASSERT( account_record.valid(), "No account for the key we just imported!?" );

  _wallet->auto_backup( "key_import" );

  return account_record->name;
}

optional<string> detail::client_impl::wallet_dump_private_key( const string& input )const
{ try {
  try
  {
     ASSERT_TASK_NOT_PREEMPTED(); // make sure no cancel gets swallowed by catch(...)
     //If input is an address...
     return utilities::key_to_wif( _wallet->get_private_key( address( input ) ) );
  }
  catch( ... )
  {
      try
      {
         ASSERT_TASK_NOT_PREEMPTED(); // make sure no cancel gets swallowed by catch(...)
         //If input is a public key...
         return utilities::key_to_wif( _wallet->get_private_key( address( public_key_type( input ) ) ) );
      }
      catch( ... )
      {
      }
  }
  return optional<string>();
} FC_CAPTURE_AND_RETHROW( (input) ) }

optional<string> detail::client_impl::wallet_dump_account_private_key( const string& account_name,
                                                                       const account_key_type& key_type )const
{ try {
    const auto account_record = _wallet->get_account( account_name );
    switch( key_type )
    {
        case owner_key:
            return utilities::key_to_wif( _wallet->get_private_key( account_record.owner_address() ) );
        case active_key:
            return utilities::key_to_wif( _wallet->get_private_key( account_record.active_address() ) );
        case signing_key:
            FC_ASSERT( account_record.is_delegate() );
            return utilities::key_to_wif( _wallet->get_private_key( account_record.signing_address() ) );
        default:
            return optional<string>();
    }
} FC_CAPTURE_AND_RETHROW( (account_name)(key_type) ) }

public_key_type client_impl::wallet_account_create( const string& account_name )
{ try {
   const auto result = _wallet->create_account( account_name );
   _wallet->auto_backup( "account_create" );
   return result;
} FC_CAPTURE_AND_RETHROW( (account_name) ) }

void client_impl::wallet_rescan_blockchain( const uint32_t start_block_num, const uint32_t limit, const bool scan_in_background )
{ try {
    _wallet->start_scan( start_block_num, limit, scan_in_background );
} FC_CAPTURE_AND_RETHROW( (start_block_num)(limit)(scan_in_background) ) }

void client_impl::wallet_cancel_scan()
{ try {
    _wallet->cancel_scan();
} FC_CAPTURE_AND_RETHROW() }

wallet_transaction_record client_impl::wallet_scan_transaction( const string& transaction_id, bool overwrite_existing )
{ try {
   return _wallet->scan_transaction( transaction_id, overwrite_existing );
} FC_RETHROW_EXCEPTIONS( warn, "", ("transaction_id",transaction_id)("overwrite_existing",overwrite_existing) ) }

void client_impl::wallet_scan_transaction_experimental( const string& transaction_id, bool overwrite_existing )
{ try {
#ifndef BTS_TEST_NETWORK
   FC_ASSERT( false, "This command is for developer testing only!" );
#endif
   _wallet->scan_transaction_experimental( transaction_id, overwrite_existing );
} FC_RETHROW_EXCEPTIONS( warn, "", ("transaction_id",transaction_id)("overwrite_existing",overwrite_existing) ) }

void client_impl::wallet_add_transaction_note_experimental( const string& transaction_id, const string& note )
{ try {
#ifndef BTS_TEST_NETWORK
   FC_ASSERT( false, "This command is for developer testing only!" );
#endif
   _wallet->add_transaction_note_experimental( transaction_id, note );
} FC_RETHROW_EXCEPTIONS( warn, "", ("transaction_id",transaction_id)("note",note) ) }

set<pretty_transaction_experimental> client_impl::wallet_transaction_history_experimental( const string& account_name )const
{ try {
#ifndef BTS_TEST_NETWORK
   FC_ASSERT( false, "This command is for developer testing only!" );
#endif
   return _wallet->transaction_history_experimental( account_name );
} FC_RETHROW_EXCEPTIONS( warn, "", ("account_name",account_name) ) }

wallet_transaction_record client_impl::wallet_get_transaction( const string& transaction_id )
{ try {
   return _wallet->get_transaction( transaction_id );
} FC_RETHROW_EXCEPTIONS( warn, "", ("transaction_id",transaction_id) ) }

wallet_transaction_record client_impl::wallet_account_register( const string& account_name,
                                                                const string& pay_with_account,
                                                                const fc::variant& data,
                                                                uint8_t delegate_pay_rate,
                                                                const string& new_account_type )
{ try {
    auto record = _wallet->register_account( account_name, data, delegate_pay_rate,
                                                   pay_with_account, variant(new_account_type).as<account_type>(),
                                                   true );
    _wallet->cache_transaction( record );
    network_broadcast_transaction( record.trx );
    return record;
} FC_RETHROW_EXCEPTIONS(warn, "", ("account_name", account_name)("data", data)) }

variant_object client_impl::wallet_get_info()
{
   return _wallet->get_info().get_object();
}

void client_impl::wallet_set_custom_data( const wallet_record_type_enum& type, const string& item, const variant_object& custom_data )
{ try {
    switch( type )
    {
        case account_record_type:
        {
            owallet_account_record record = wallet_get_account( item );
            if( !record.valid() ) break;
            record->custom_data = custom_data;
            _wallet->store_account( *record );
            break;
        }
        case contact_record_type:
        {
            owallet_contact_record record = wallet_get_contact( item );
            if( !record.valid() ) break;
            record->custom_data = custom_data;
            _wallet->store_contact( *record );
            break;
        }
        case approval_record_type:
        {
            owallet_approval_record record = wallet_get_approval( item );
            if( !record.valid() ) break;
            record->custom_data = custom_data;
            _wallet->store_approval( *record );
            break;
        }
        default:
            break;
    }
} FC_CAPTURE_AND_RETHROW( (type)(item)(custom_data) ) }

wallet_transaction_record client_impl::wallet_account_update_registration(
        const string& account_to_update,
        const string& pay_from_account,
        const variant& public_data,
        uint8_t delegate_pay_rate )
{
   auto record = _wallet->update_registered_account( account_to_update, pay_from_account, public_data, delegate_pay_rate, true );
   _wallet->cache_transaction( record );
   network_broadcast_transaction( record.trx );
   return record;
}

wallet_transaction_record detail::client_impl::wallet_account_update_active_key( const std::string& account_to_update,
                                                                                 const std::string& pay_from_account,
                                                                                 const std::string& new_active_key )
{
   auto record = _wallet->update_active_key( account_to_update, pay_from_account, new_active_key, true );
   _wallet->cache_transaction( record );
   network_broadcast_transaction( record.trx );
   return record;
}

wallet_transaction_record detail::client_impl::wallet_account_retract( const std::string& account_to_update,
                                                                       const std::string& pay_from_account )
{
   auto record = _wallet->retract_account( account_to_update, pay_from_account, true );
   _wallet->cache_transaction( record );
   network_broadcast_transaction( record.trx );
   return record;
}

vector<public_key_summary> client_impl::wallet_account_list_public_keys( const string& account_name )
{
    vector<public_key_summary> summaries;
    vector<public_key_type> keys = _wallet->get_public_keys_in_account( account_name );
    summaries.reserve( keys.size() );
    for( const auto& key : keys )
    {
        summaries.push_back(_wallet->get_public_key_summary( key ));
    }
    return summaries;
}

vector<bts::wallet::escrow_summary> client_impl::wallet_escrow_summary( const string& account_name ) const
{ try {
   return _wallet->get_escrow_balances( account_name );
} FC_CAPTURE_AND_RETHROW( (account_name) ) }

account_balance_summary_type client_impl::wallet_account_balance( const string& account_name )const
{ try {
    return _wallet->get_spendable_account_balances( account_name );
} FC_CAPTURE_AND_RETHROW( (account_name) ) }

account_balance_id_summary_type client_impl::wallet_account_balance_ids( const string& account_name )const
{ try {
    return _wallet->get_account_balance_ids( account_name );
} FC_CAPTURE_AND_RETHROW( (account_name) ) }

account_extended_balance_type client_impl::wallet_account_balance_extended( const string& account_name )const
{
#if 0
    const map<string, vector<balance_record>>& balance_records = _wallet->get_account_balance_records( account_name, false, -1 );

    map<string, map<string, map<asset_id_type, share_type>>> raw_balances;
    for( const auto& item : balance_records )
    {
        const string& account_name = item.first;
        for( const auto& balance_record : item.second )
        {
            string type_label;
            if( balance_record.snapshot_info.valid() )
            {
                switch( withdraw_condition_types( balance_record.condition.type ) )
                {
                    case withdraw_signature_type:
                        type_label = "GENESIS";
                        break;
                    case withdraw_vesting_type:
                        type_label = "SHAREDROP";
                        break;
                    default:
                        break;
                }
            }
            if( type_label.empty() )
                type_label = balance_record.condition.type_label();

            const asset& balance = balance_record.get_spendable_balance( _chain_db->get_pending_state()->now() );
            raw_balances[ account_name ][ type_label ][ balance.asset_id ] += balance.amount;
        }
    }

    map<string, map<string, vector<asset>>> extended_balances;
    for( const auto& item : raw_balances )
    {
        const string& account_name = item.first;
        for( const auto& type_item : item.second )
        {
            const string& type_label = type_item.first;
            for( const auto& balance_item : type_item.second )
            {
                extended_balances[ account_name ][ type_label ].emplace_back( balance_item.second, balance_item.first );
            }
        }
    }

    return extended_balances;
#endif
    return account_extended_balance_type();
}

account_vesting_balance_summary_type client_impl::wallet_account_vesting_balances( const string& account_name )const
{ try {
    return _wallet->get_account_vesting_balances( account_name );
} FC_CAPTURE_AND_RETHROW( (account_name) ) }

account_balance_summary_type client_impl::wallet_account_yield( const string& account_name )const
{
  return _wallet->get_account_yield( account_name );
}

wallet_transaction_record client_impl::wallet_market_submit_bid(
       const string& from_account,
       const string& quantity,
       const string& quantity_symbol,
       const string& quote_price,
       const string& quote_symbol,
       bool allow_stupid_bid )
{
  vector<market_order> lowest_ask = blockchain_market_order_book(quote_symbol, quantity_symbol, 1).second;

  if( (!allow_stupid_bid) && (!lowest_ask.empty()) )
  {
      const double requested_price = variant( quote_price ).as_double();
      const double lowest_ask_price = variant( _chain_db->to_pretty_price( lowest_ask.front().get_price(), false ) ).as_double();
      if( (requested_price > lowest_ask_price * 1.05) )
      {
          FC_THROW_EXCEPTION(stupid_order, "You are attempting to bid at more than 5% above the buy price. "
                                           "This bid is based on economically unsound principles, and is ill-advised. "
                                           "If you're sure you want to do this, place your bid again and set allow_stupid_bid to true. ${lowest_ask}",
                                           ("lowest_ask",lowest_ask.front()));
      }
  }

  auto record = _wallet->submit_bid( from_account, quantity, quantity_symbol, quote_price, quote_symbol, true );
  _wallet->cache_transaction( record );
  network_broadcast_transaction( record.trx );
  return record;
}

wallet_transaction_record client_impl::wallet_market_submit_ask(
           const string& from_account,
           const string& quantity,
           const string& quantity_symbol,
           const string& quote_price,
           const string& quote_symbol,
           bool allow_stupid_ask )
{
  vector<market_order> highest_bid = blockchain_market_order_book(quote_symbol, quantity_symbol, 1).first;

  if( (!allow_stupid_ask) && (!highest_bid.empty()) )
  {
      const double requested_price = variant( quote_price ).as_double();
      const double highest_bid_price = variant( _chain_db->to_pretty_price( highest_bid.front().get_price(), false ) ).as_double();
      if( requested_price < highest_bid_price * 0.95 )
      {
          FC_THROW_EXCEPTION(stupid_order, "You are attempting to ask at more than 5% below the buy price. "
                                           "This ask is based on economically unsound principles, and is ill-advised. "
                                           "If you're sure you want to do this, place your ask again and set allow_stupid_ask to true.");
      }
  }

  auto record = _wallet->submit_ask( from_account, quantity, quantity_symbol, quote_price, quote_symbol, true );
  _wallet->cache_transaction( record );
  network_broadcast_transaction( record.trx );
  return record;
}

wallet_transaction_record client_impl::wallet_market_submit_short(
       const string& from_account,
       const string& quantity,
       const string& collateral_symbol,
       const string& apr,
       const string& quote_symbol,
       const string& short_price_limit )
{
  auto record = _wallet->submit_short( from_account,
                                             quantity,
                                             collateral_symbol,
                                             apr,
                                             quote_symbol,
                                             short_price_limit,
                                             true );
  _wallet->cache_transaction( record );
  network_broadcast_transaction( record.trx );
  return record;
}

wallet_transaction_record client_impl::wallet_market_batch_update(const std::vector<order_id_type> &cancel_order_ids,
                                                                 const std::vector<order_description> &new_orders,
                                                                 bool sign)
{
  auto record = _wallet->batch_market_update(cancel_order_ids, new_orders, sign);
  if( sign )
  {
     _wallet->cache_transaction( record );
     network_broadcast_transaction( record.trx );
  }
  return record;
}

wallet_transaction_record client_impl::wallet_market_cover(
       const string& from_account,
       const string& quantity,
       const string& quantity_symbol,
       const order_id_type& cover_id )
{
  auto record = _wallet->cover_short( from_account, quantity, quantity_symbol, cover_id, true );
  _wallet->cache_transaction( record );
  network_broadcast_transaction( record.trx );
  return record;
}

wallet_transaction_record client_impl::wallet_delegate_withdraw_pay( const string& delegate_name,
                                                                     const string& to_account_name,
                                                                     const string& amount_to_withdraw )
{
  const oasset_record base_record = _chain_db->get_asset_record( asset_id_type( 0 ) );
  FC_ASSERT( base_record.valid() );
  const asset amount = _chain_db->to_ugly_asset( amount_to_withdraw, base_record->symbol );
  auto record = _wallet->withdraw_delegate_pay( delegate_name, amount, to_account_name, true );
  _wallet->cache_transaction( record );
  network_broadcast_transaction( record.trx );
  return record;
}

asset client_impl::wallet_set_transaction_fee( const string& fee )
{ try {
  oasset_record asset_record = _chain_db->get_asset_record( asset_id_type() );
  FC_ASSERT( asset_record.valid() );
  const asset amount = _chain_db->to_ugly_asset( fee, asset_record->symbol );
  _wallet->set_transaction_fee( amount );
  return _wallet->get_transaction_fee();
} FC_CAPTURE_AND_RETHROW( (fee) ) }

asset client_impl::wallet_get_transaction_fee( const string& fee_symbol )
{
  if( fee_symbol.empty() )
     return _wallet->get_transaction_fee( _chain_db->get_asset_id( BTS_BLOCKCHAIN_SYMBOL ) );
  return _wallet->get_transaction_fee( _chain_db->get_asset_id( fee_symbol ) );
}

wallet_transaction_record client_impl::wallet_market_add_collateral( const std::string& from_account_name,
                                                                     const order_id_type& cover_id,
                                                                     const string& real_quantity_collateral_to_add )
{
   auto record = _wallet->add_collateral( from_account_name, cover_id, real_quantity_collateral_to_add, true );
   _wallet->cache_transaction( record );
   network_broadcast_transaction( record.trx );
   return record;
}

map<order_id_type, market_order> client_impl::wallet_account_order_list( const string& account_name,
                                                                         uint32_t limit )
{
   return _wallet->get_market_orders( account_name, "", "", limit );
}

map<order_id_type, market_order> client_impl::wallet_market_order_list( const string& quote_symbol,
                                                                        const string& base_symbol,
                                                                        uint32_t limit,
                                                                        const string& account_name )
{
   return _wallet->get_market_orders( account_name, quote_symbol, base_symbol, limit );
}

wallet_transaction_record client_impl::wallet_market_cancel_order( const order_id_type& order_id )
{
   auto record = _wallet->cancel_market_orders( {order_id}, true );
   _wallet->cache_transaction( record );
   network_broadcast_transaction( record.trx );
   return record;
}

wallet_transaction_record client_impl::wallet_market_cancel_orders( const vector<order_id_type>& order_ids )
{
   auto record = _wallet->cancel_market_orders( order_ids, true );
   _wallet->cache_transaction( record );
   network_broadcast_transaction( record.trx );
   return record;
}

account_vote_summary_type client_impl::wallet_account_vote_summary( const string& account_name )const
{
   if( !account_name.empty() && !_chain_db->is_valid_account_name( account_name ) )
       FC_CAPTURE_AND_THROW( invalid_account_name, (account_name) );

   return _wallet->get_account_vote_summary( account_name );
}

void client_impl::wallet_delegate_set_block_production( const string& delegate_name, bool enabled )
{
   _wallet->set_delegate_block_production( delegate_name, enabled );
   reschedule_delegate_loop();
}

bool client_impl::wallet_set_transaction_scanning( bool enabled )
{
    _wallet->set_transaction_scanning( enabled );
    return _wallet->get_transaction_scanning();
}

fc::ecc::compact_signature client_impl::wallet_sign_hash(const string& signer, const fc::sha256& hash)
{
    return _wallet->sign_hash( signer, hash );
}

std::string client_impl::wallet_login_start(const std::string &server_account)
{
   return _wallet->login_start(server_account);
}

fc::variant client_impl::wallet_login_finish(const public_key_type &server_key,
                                             const public_key_type &client_key,
                                             const fc::ecc::compact_signature &client_signature)
{
   return _wallet->login_finish(server_key, client_key, client_signature);
}


transaction_builder client_impl::wallet_balance_set_vote_info(const balance_id_type& balance_id,
                                                              const string& voter_address,
                                                              const vote_strategy& strategy,
                                                              bool sign_and_broadcast,
                                                              const string& builder_path )
{
    address new_voter;
    if( voter_address == "" )
    {
        auto balance = _chain_db->get_balance_record( balance_id );
        if( balance.valid() && balance->restricted_owner.valid() )
            new_voter = *balance->restricted_owner;
        else
            FC_ASSERT(false, "Didn't specify a voter address and none currently exists.");
    }
    else
    {
        new_voter = address( voter_address );
    }
    auto builder = _wallet->create_transaction_builder( _wallet->set_vote_info( balance_id, new_voter, strategy ) );
    if( sign_and_broadcast )
    {
        auto record = builder->sign();
        _wallet->cache_transaction( record );
        network_broadcast_transaction( record.trx );
    }
    _wallet->write_latest_builder( *builder, builder_path );
    return *builder;
}


wallet_transaction_record client_impl::wallet_publish_price_feed( const string& delegate_account,
                                                                  const string& real_amount_per_xts,
                                                                  const string& real_amount_symbol )
{
   const oasset_record base_record = _chain_db->get_asset_record( asset_id_type( 0 ) );
   FC_ASSERT( base_record.valid() );

   const price new_price = _chain_db->to_ugly_price( real_amount_per_xts, base_record->symbol, real_amount_symbol );

   auto record = _wallet->publish_price( delegate_account, new_price, true );
   _wallet->cache_transaction( record );
   network_broadcast_transaction( record.trx );

   return record;
}

vector<std::pair<string, wallet_transaction_record>> client_impl::wallet_publish_feeds_multi_experimental(
        const map<string,string>& amount_per_xts )
{
   vector<std::pair<string, wallet_transaction_record>> record_list =
      _wallet->publish_feeds_multi_experimental( amount_per_xts, true );

   for( std::pair<string, wallet_transaction_record>& record_pair : record_list )
   {
      wallet_transaction_record& record = record_pair.second;
      _wallet->cache_transaction( record );
      network_broadcast_transaction( record.trx );
   }
   return record_list;
}

wallet_transaction_record client_impl::wallet_publish_feeds( const std::string& delegate_account,
                                                             const map<string,string>& amount_per_xts )
{
   auto record = _wallet->publish_feeds( delegate_account, amount_per_xts, true );
   _wallet->cache_transaction( record );
   network_broadcast_transaction( record.trx );
   return record;
}

void client_impl::wallet_repair_records( const string& collecting_account_name )
{ try {
   _wallet->auto_backup( "before_record_repair" );
   optional<string> account_name;
   if( !collecting_account_name.empty() )
       account_name = collecting_account_name;
   return _wallet->repair_records( account_name );
} FC_CAPTURE_AND_RETHROW( (collecting_account_name) ) }

int32_t client_impl::wallet_regenerate_keys( const std::string& account, uint32_t number_to_regenerate )
{
   _wallet->auto_backup( "before_key_regeneration" );
   return _wallet->regenerate_keys( account, number_to_regenerate );
}
string client_impl::wallet_generate_brain_seed()const
{
   string result;
   auto priv_key = fc::ecc::private_key::generate();
   auto secret = priv_key.get_secret();

   uint16_t* keys = (uint16_t*)&secret;
   for( uint32_t i = 0; i < 9; ++i )
      result += word_list[keys[i]%word_list_size] + string(" ");

   result += string( address(priv_key.get_public_key()) ).substr(4);

   return result;
}

vector<wallet_contact_record> client_impl::wallet_list_contacts()const
{ try {
    return _wallet->list_contacts();
} FC_CAPTURE_AND_RETHROW() }

owallet_contact_record client_impl::wallet_get_contact( const string& contact )const
{ try {
    if( contact.find( "label:" ) == 0 )
        return _wallet->lookup_contact( contact.substr( string( "label:" ).size() ) );
    else
        return _wallet->lookup_contact( variant( contact ) );
} FC_CAPTURE_AND_RETHROW( (contact) ) }

wallet_contact_record client_impl::wallet_add_contact( const string& contact, const string& label )
{ try {
    return _wallet->store_contact( contact_data( *_chain_db, contact, label) );
} FC_CAPTURE_AND_RETHROW( (contact)(label) ) }

owallet_contact_record client_impl::wallet_remove_contact( const string& contact )
{ try {
    if( contact.find( "label:" ) == 0 )
        return _wallet->remove_contact( contact.substr( string( "label:" ).size() ) );
    else
        return _wallet->remove_contact( variant( contact ) );
} FC_CAPTURE_AND_RETHROW( (contact) ) }

vector<wallet_approval_record> client_impl::wallet_list_approvals()const
{ try {
    return _wallet->list_approvals();
} FC_CAPTURE_AND_RETHROW() }

owallet_approval_record client_impl::wallet_get_approval( const string& approval )const
{ try {
    return _wallet->lookup_approval( approval );
} FC_CAPTURE_AND_RETHROW( (approval) ) }

wallet_approval_record client_impl::wallet_approve( const string& name, int8_t approval )
{ try {
    return _wallet->store_approval( approval_data( *_chain_db, name, approval) );
} FC_CAPTURE_AND_RETHROW( (name)(approval) ) }

} } } // namespace bts::client::detail
