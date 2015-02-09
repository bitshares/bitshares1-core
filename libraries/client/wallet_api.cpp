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

int8_t detail::client_impl::wallet_account_set_approval( const string& account_name, int8_t approval )
{ try {
  _wallet->set_account_approval( account_name, approval );
  return _wallet->get_account_approval( account_name );
} FC_RETHROW_EXCEPTIONS( warn, "", ("account_name",account_name)("approval",approval) ) }

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


void detail::client_impl::wallet_create(const string& wallet_name, const string& password, const string& brain_key)
{
  string trimmed_name = fc::trim(wallet_name);
  if( brain_key.size() && brain_key.size() < BTS_WALLET_MIN_BRAINKEY_LENGTH ) FC_CAPTURE_AND_THROW( brain_key_too_short );
  if( password.size() < BTS_WALLET_MIN_PASSWORD_LENGTH ) FC_CAPTURE_AND_THROW( password_too_short );
  if( trimmed_name.size() == 0 ) FC_CAPTURE_AND_THROW( fc::invalid_arg_exception, (trimmed_name) );
  _wallet->create(trimmed_name,password, brain_key );
  reschedule_delegate_loop();
}

fc::optional<string> detail::client_impl::wallet_get_name() const
{
  return _wallet->is_open() ? _wallet->get_wallet_name() : fc::optional<string>();
}

void detail::client_impl::wallet_close()
{
    if (_wallet) {
        _wallet->close();
        reschedule_delegate_loop();
    }
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

// This should be able to get an encrypted private key or WIF key out of any reasonable JSON object
void read_keys( const fc::variant& vo, vector<private_key_type>& keys, const string& password )
{
    ilog("@n read_keys");
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


void detail::client_impl::wallet_change_passphrase(const string& new_password)
{
  _wallet->auto_backup( "passphrase_change" );
  _wallet->change_passphrase(new_password);
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
wallet_transaction_record detail::client_impl::wallet_asset_authorize_key( const string& paying_account_name,
                                                                           const string& symbol,
                                                                           const string& key,
                                                                           const object_id_type& meta )
{
   address addr;
   try {
      try { addr = address( public_key_type( key ) ); }
      catch ( ... ) { addr = address( key ); }
   }
   catch ( ... )
   {
      auto account = _chain_db->get_account_record( key );
      FC_ASSERT( account.valid() );
      addr = account->active_key();
   }
   auto record = _wallet->asset_authorize_key( paying_account_name, symbol, addr, meta, true );
   _wallet->cache_transaction( record );
   network_broadcast_transaction( record.trx );
   return record;
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

wallet_transaction_record detail::client_impl::wallet_recover_transaction( const string& transaction_id_prefix,
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
        const string& to_account_name,
        const string& memo_message,
        const vote_strategy& strategy )
{
    return wallet_transfer_from(amount_to_transfer, asset_symbol, from_account_name, from_account_name,
                                to_account_name, memo_message, strategy);
}

wallet_transaction_record detail::client_impl::wallet_transfer_to_public_account(
        double amount_to_transfer,
        const string& asset_symbol,
        const string& from_account_name,
        const string& to_account_name,
        const string& memo_message,
        const vote_strategy& strategy )
{
    const oaccount_record account_record = _chain_db->get_account_record( to_account_name );
    FC_ASSERT( account_record.valid() && !account_record->is_retracted() );
    auto record = _wallet->transfer_asset_to_address(amount_to_transfer,
                                                     asset_symbol,
                                                     from_account_name,
                                                     account_record->active_address(),
                                                     memo_message,
                                                     strategy,
                                                     true );
    _wallet->cache_transaction( record );
    network_broadcast_transaction( record.trx );
    return record;
}

wallet_transaction_record detail::client_impl::wallet_burn(
        double amount_to_transfer,
        const string& asset_symbol,
        const string& from_account_name,
        const string& for_or_against,
        const string& to_account_name,
        const string& public_message,
        bool anonymous )
{
    auto record = _wallet->burn_asset( amount_to_transfer, asset_symbol,
                                             from_account_name, for_or_against, to_account_name,
                                             public_message, anonymous, true );
    _wallet->cache_transaction( record );
    network_broadcast_transaction( record.trx );
    return record;
}


string  detail::client_impl::wallet_address_create( const string& account_name,
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


wallet_transaction_record detail::client_impl::wallet_transfer_to_legacy_address(
        double amount_to_transfer,
        const string& asset_symbol,
        const string& from_account_name,
        const pts_address& to_address,
        const string& memo_message,
        const vote_strategy& strategy )
{
    auto record =  _wallet->transfer_asset_to_address( amount_to_transfer,
                                                       asset_symbol,
                                                       from_account_name,
                                                       address( to_address ),
                                                       memo_message,
                                                       strategy,
                                                       true );
    _wallet->cache_transaction( record );
    network_broadcast_transaction( record.trx );
    return record;

}



wallet_transaction_record detail::client_impl::wallet_transfer_to_address(
        double amount_to_transfer,
        const string& asset_symbol,
        const string& from_account_name,
        const string& to_address,
        const string& memo_message,
        const vote_strategy& strategy )
{
    address effective_address;
    if( address::is_valid( to_address ) )
        effective_address = address( to_address );
    else
        effective_address = address( public_key_type( to_address ) );
    auto record =  _wallet->transfer_asset_to_address( amount_to_transfer,
                                                       asset_symbol,
                                                       from_account_name,
                                                       effective_address,
                                                       memo_message,
                                                       strategy,
                                                       true );
    _wallet->cache_transaction( record );
    network_broadcast_transaction( record.trx );
    return record;

}

wallet_transaction_record detail::client_impl::wallet_transfer_from(
        const string& amount_to_transfer,
        const string& asset_symbol,
        const string& paying_account_name,
        const string& from_account_name,
        const string& to_account_name,
        const string& memo_message,
        const vote_strategy& strategy )
{
    asset amount = _chain_db->to_ugly_asset(amount_to_transfer, asset_symbol);
    auto payer = _wallet->get_account(paying_account_name);
    auto recipient = _wallet->get_account(to_account_name);
    transaction_builder_ptr builder = _wallet->create_transaction_builder();
    auto record = builder->deposit_asset(payer, recipient, amount,
                                         memo_message, from_account_name)
                          .finalize( true, strategy )
                          .sign();

    _wallet->cache_transaction( record );
    network_broadcast_transaction( record.trx );

    if( _mail_client )
    {
        for( auto&& notice : builder->encrypted_notifications() )
            _mail_client->send_encrypted_message(std::move(notice),
                                                 from_account_name,
                                                 to_account_name,
                                                 recipient.owner_key);
    }

    return record;
}

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


wallet_transaction_record detail::client_impl::wallet_object_create(
                                            const string& account,
                                            const variant& user_data,
                                            int32_t m,
                                            const vector<address>& owners )
{ try {
    auto builder = _wallet->create_transaction_builder();
    object_record obj( obj_type::base_object, 0 );
    vector<address> real_owners = owners;
    if( m == -1 ) // default value - can't use 0 because 0 is a valid number of owners
    {
        m = 1;
        real_owners = vector<address>{ _wallet->create_new_address( account, "Owner address for an object." ) };
    }
    obj.user_data = user_data;
    obj._owners = multisig_condition( m, set<address>(real_owners.begin(), real_owners.end()) );
    builder->set_object( account, obj, true )
            .finalize( )
            .sign();
    network_broadcast_transaction( builder->transaction_record.trx );
    return builder->transaction_record;
} FC_CAPTURE_AND_RETHROW( (account)(m)(owners)(user_data) ) }

transaction_builder detail::client_impl::wallet_object_update(
                                            const string& paying_account,
                                            const object_id_type& id,
                                            const variant& user_data,
                                            bool sign_and_broadcast )
{ try {
    auto builder = _wallet->create_transaction_builder();
    auto obj = _chain_db->get_object_record( id );
    FC_ASSERT( obj.valid(), "No such object!" );
    obj->user_data = user_data;
    builder->set_object( paying_account, *obj, true )
            .finalize();
    if( sign_and_broadcast )
    {
        builder->sign();
        network_broadcast_transaction( builder->transaction_record.trx );
    }
    return *builder;
} FC_CAPTURE_AND_RETHROW( (paying_account)(id)(user_data)(sign_and_broadcast ) ) }

transaction_builder detail::client_impl::wallet_object_transfer(
                                            const string& paying_account,
                                            const object_id_type& id,
                                            uint32_t m,
                                            const vector<address>& owners,
                                            bool sign_and_broadcast )
{ try {
    auto builder = _wallet->create_transaction_builder();
    auto obj = _chain_db->get_object_record( id );
    FC_ASSERT( obj.valid(), "No such object!" );
    obj->_owners = multisig_condition( m, set<address>(owners.begin(), owners.end()) );
    builder->set_object( paying_account, *obj, true )
            .finalize();
    if( sign_and_broadcast )
    {
        builder->sign();
        network_broadcast_transaction( builder->transaction_record.trx );
    }
    return *builder;
} FC_CAPTURE_AND_RETHROW( (paying_account)(id)(m)(owners)(sign_and_broadcast) ) }


vector<object_record> detail::client_impl::wallet_object_list( const string& account )
{ try {
    ilog("@n called wallet_object_list");
    vector<object_record> ret;
    const auto pending_state = _chain_db->get_pending_state();
    const auto acct_keys = _wallet->get_public_keys_in_account( account );
    const auto scan_object = [&]( const object_record& obj )
    {
        ilog("@n scan_object callback for object: ${o}", ("o", obj));
        auto condition = pending_state->get_object_condition( obj );
        ilog("@n condition is: ${c}", ("c", condition));
        for( auto owner : condition.owners )
        {
            for( auto key : acct_keys )
            {
                if( address(key) == owner )
                    ret.push_back( obj );
            }
        }
    };
    _chain_db->scan_objects( scan_object );
    return ret;
} FC_CAPTURE_AND_RETHROW( (account) ) }

transaction_builder detail::client_impl::wallet_set_edge(
                                            const string& paying_account,
                                            const object_id_type& from,
                                            const object_id_type& to,
                                            const string& name,
                                            const variant& value )
{ try {
    auto builder = _wallet->create_transaction_builder();
    edge_record edge;
    edge.from = from;
    edge.to = to;
    edge.name = name;
    edge.value = value;

    builder->set_edge( paying_account, edge );
    builder->finalize().sign();
    network_broadcast_transaction( builder->transaction_record.trx );
    return *builder;
} FC_CAPTURE_AND_RETHROW( (paying_account)(from)(to)(name)(value) ) }



wallet_transaction_record detail::client_impl::wallet_release_escrow( const string& paying_account_name,
                                                                      const address& escrow_balance_id,
                                                                      const string& released_by,
                                                                      double amount_to_sender,
                                                                      double amount_to_receiver )
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
    if( asset_rec->precision )
    {
       amount_to_sender   *= asset_rec->precision;
       amount_to_receiver *= asset_rec->precision;
    }

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

    if( _mail_client )
    {
        /* TODO: notify other parties of the transaction.
        for( auto&& notice : builder->encrypted_notifications() )
            _mail_client->send_encrypted_message(std::move(notice),
                                                 from_account_name,
                                                 to_account_name,
                                                 recipient.owner_key);

        */
    }

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
    asset amount = _chain_db->to_ugly_asset(amount_to_transfer, asset_symbol);
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

    if( _mail_client )
    {
        for( auto&& notice : builder->encrypted_notifications() )
            _mail_client->send_encrypted_message(std::move(notice),
                                                 from_account_name,
                                                 to_account_name,
                                                 recipient.owner_key);
    }

    return record;
}

wallet_transaction_record detail::client_impl::wallet_asset_create(
        const string& symbol,
        const string& asset_name,
        const string& issuer_name,
        const string& description,
        double maximum_share_supply ,
        uint64_t precision,
        const variant& public_data,
        bool is_market_issued /* = false */ )
{
  auto record = _wallet->create_asset( symbol, asset_name, description, public_data, issuer_name,
                                             maximum_share_supply, precision, is_market_issued, true );
  _wallet->cache_transaction( record );
  network_broadcast_transaction( record.trx );
  return record;
}

wallet_transaction_record detail::client_impl::wallet_asset_update(
        const string& symbol,
        const optional<string>& name,
        const optional<string>& description,
        const optional<variant>& public_data,
        const optional<double>& maximum_share_supply,
        const optional<uint64_t>& precision,
        const share_type& issuer_fee,
        double issuer_market_fee,
        const vector<asset_permissions>& flags,
        const vector<asset_permissions>& issuer_permissions,
        const string& issuer_account_name,
        uint32_t required_sigs,
        const vector<address>& authority
      )
{
   uint32_t flags_int = 0;
   uint32_t issuer_perms_int = 0;
   for( auto item : flags ) flags_int |= item;
   for( auto item : issuer_permissions ) issuer_perms_int |= item;
   auto record = _wallet->update_asset( symbol, name, description, public_data, maximum_share_supply,
                                        precision, issuer_fee, issuer_market_fee, flags_int,
                                        issuer_perms_int, issuer_account_name,
                                        required_sigs, authority, true );

   _wallet->cache_transaction( record );
   network_broadcast_transaction( record.trx );
   return record;
}

wallet_transaction_record detail::client_impl::wallet_asset_issue(
        double real_amount,
        const string& symbol,
        const string& to_account_name,
        const string& memo_message )
{
  auto record = _wallet->issue_asset( real_amount, symbol, to_account_name, memo_message, true );
  _wallet->cache_transaction( record );
  network_broadcast_transaction( record.trx );
  return record;
}

wallet_transaction_record detail::client_impl::wallet_asset_issue_to_addresses(
        const string& symbol,
        const map<string, share_type>& addresses )
{
  auto record = _wallet->issue_asset_to_addresses( symbol, addresses );
  _wallet->cache_transaction( record );
  network_broadcast_transaction( record.trx );
  return record;
}


vector<string> detail::client_impl::wallet_list() const
{
  return _wallet->list();
}

vector<wallet_account_record> detail::client_impl::wallet_list_accounts() const
{
  return _wallet->list_accounts();
}

vector<wallet_account_record> detail::client_impl::wallet_list_my_accounts() const
{
  return _wallet->list_my_accounts();
}

vector<wallet_account_record> detail::client_impl::wallet_list_favorite_accounts() const
{
  return _wallet->list_favorite_accounts();
}

vector<wallet_account_record> detail::client_impl::wallet_list_unregistered_accounts() const
{
  return _wallet->list_unregistered_accounts();
}

void detail::client_impl::wallet_remove_contact_account(const string& account_name)
{
  _wallet->remove_contact_account( account_name );
}

void detail::client_impl::wallet_account_rename(const string& current_account_name,
                                   const string& new_account_name)
{
  _wallet->rename_account(current_account_name, new_account_name);
  _wallet->auto_backup( "account_rename" );
}

wallet_account_record detail::client_impl::wallet_get_account(const string& account_name) const
{ try {
  return _wallet->get_account( account_name );
} FC_RETHROW_EXCEPTIONS( warn, "", ("account_name",account_name) ) }

address detail::client_impl::wallet_get_account_public_address(const string& account_name) const
{ try {
  auto acct = _wallet->get_account( account_name );
  return acct.owner_address();
} FC_RETHROW_EXCEPTIONS( warn, "", ("account_name",account_name) ) }


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
      ilog( "import_bitcoin_wallet failed with empty password: ${e}", ("e",e.to_detail_string() ) );
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

bts::mail::message detail::client_impl::wallet_mail_create(const std::string& sender,
                                                           const std::string& subject,
                                                           const std::string& body,
                                                           const bts::mail::message_id_type& reply_to)
{
    return _wallet->mail_create(sender, subject, body, reply_to);
}

bts::mail::message detail::client_impl::wallet_mail_encrypt(const std::string &recipient, const bts::mail::message &plaintext)
{
    auto recipient_account = _chain_db->get_account_record(recipient);
    FC_ASSERT(recipient_account, "Unknown recipient name.");

    return _wallet->mail_encrypt(recipient_account->active_key(), plaintext);
}

bts::mail::message detail::client_impl::wallet_mail_open(const address& recipient, const bts::mail::message& ciphertext)
{
    return _wallet->mail_open(recipient, ciphertext);
}

void detail::client_impl::wallet_set_preferred_mail_servers(const string& account_name,
                                                            const std::vector<string>& server_list,
                                                            const string& paying_account)
{
    /*
     * Brief overview of what's going on here... the user specifies a list of blockchain accounts which host mail
     * servers. These are the mail servers the user's client will check for new mail. This list is published as a
     * list of strings in public_data["mail_servers"].
     *
     * The server accounts will publish the actual endpoints in their public data as an ip::endpoint object in
     * public_data["mail_server_endpoint"]. This function will not let the user set his mail server list unless all
     * of his mail servers all publish this endpoint.
     */

    if( !_wallet->is_open() )
        FC_THROW_EXCEPTION( wallet_closed, "Wallet is not open; cannot set preferred mail servers." );
    //Skip unlock check here; wallet_account_update_registration below will check it.

    //Sanity check account_name
    wallet_account_record account_rec = _wallet->get_account(account_name);
    if( !account_rec.is_my_account )
        FC_THROW_EXCEPTION( unknown_receive_account,
                            "Account ${name} is not owned by this wallet. Cannot set his mail servers.",
                            ("name", account_name) );

    //Check that all names in server_list are valid accounts which publish mail server endpoints
    for( const string& server_name : server_list )
    {
        oaccount_record server_account = _chain_db->get_account_record(server_name);
        if( !server_account )
            FC_THROW_EXCEPTION( unknown_account_name,
                                "The server account ${name} from server_list is not registered on the blockchain.",
                                ("name", server_name) );
        try {
            //Not actually storing the value; I don't care. I just want to make sure that both of these .as() calls
            //work smoothly.
            server_account->public_data.as<fc::variant_object>()["mail_server_endpoint"].as<fc::ip::endpoint>();
        } catch (fc::exception&) {
            FC_ASSERT( false,
                       "The server account ${name} from server_list does not publish a valid mail server endpoint.",
                       ("name", server_name) );
        }
    }

    //Update the public_data
    fc::mutable_variant_object public_data;
    try {
        if( !account_rec.public_data.is_null() )
            public_data = account_rec.public_data.as<fc::mutable_variant_object>();
    } catch (fc::exception&) {
        FC_ASSERT( false, "Account's public data is not an object. Please unset it or make it an object." );
    }
    public_data["mail_servers"] = server_list;

    //Commit the change
    wallet_account_update_registration(account_name, paying_account, public_data);
}

void client_impl::wallet_add_contact_account( const string& account_name,
                                              const public_key_type& contact_key )
{
   _wallet->add_contact_account( account_name, contact_key );
   _wallet->auto_backup( "account_add" );
}

public_key_type client_impl::wallet_account_create( const string& account_name,
                                                    const variant& private_data )
{
   const auto result = _wallet->create_account( account_name, private_data );
   _wallet->auto_backup( "account_create" );
   return result;
}

void client_impl::wallet_account_set_favorite( const string& account_name, bool is_favorite )
{
    _wallet->account_set_favorite( account_name, is_favorite );
}

void client_impl::wallet_rescan_blockchain( const uint32_t start_block_num, const uint32_t limit )
{ try {
    _wallet->start_scan( start_block_num, limit );
} FC_CAPTURE_AND_RETHROW( (start_block_num)(limit) ) }

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

void client_impl::wallet_account_update_private_data( const string& account_to_update,
                                                      const variant& private_data )
{
   _wallet->update_account_private_data(account_to_update, private_data);
}

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
  wdump((quote_symbol)(quantity_symbol));
  vector<market_order> lowest_ask = blockchain_market_order_book(quote_symbol, quantity_symbol, 1).second;
  wdump((lowest_ask));

  if (!allow_stupid_bid && lowest_ask.size()
      && fc::variant(quote_price).as_double() > _chain_db->to_pretty_price_double(lowest_ask.front().get_price()) * 1.05)
    FC_THROW_EXCEPTION(stupid_order, "You are attempting to bid at more than 5% above the buy price. "
                                     "This bid is based on economically unsound principles, and is ill-advised. "
                                     "If you're sure you want to do this, place your bid again and set allow_stupid_bid to true. ${lowest_ask}",
                                     ("lowest_ask",lowest_ask.front()));

  auto record = _wallet->submit_bid( from_account, quantity, quantity_symbol, quote_price, quote_symbol, true );
  _wallet->cache_transaction( record );
  network_broadcast_transaction( record.trx );
  return record;
}
wallet_transaction_record client_impl::wallet_market_sell(
       const string& from_account,
       const string& sell_quantity,
       const string& sell_quantity_symbol,
       const string& price_limit,
       const string& price_symbol,
       const string& relative_price,
       bool allow_stupid
       )
{
  auto record = _wallet->sell( from_account, sell_quantity, sell_quantity_symbol, price_limit, price_symbol, relative_price, allow_stupid, true );
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

  if (!allow_stupid_ask && highest_bid.size()
      && fc::variant(quote_price).as_double() < _chain_db->to_pretty_price_double(highest_bid.front().get_price()) * .95)
    FC_THROW_EXCEPTION(stupid_order, "You are attempting to ask at more than 5% below the buy price. "
                                     "This ask is based on economically unsound principles, and is ill-advised. "
                                     "If you're sure you want to do this, place your ask again and set allow_stupid_ask to true.");

  auto record = _wallet->submit_ask( from_account, quantity, quantity_symbol, quote_price, quote_symbol, true );
  _wallet->cache_transaction( record );
  network_broadcast_transaction( record.trx );
  return record;
}
wallet_transaction_record client_impl::wallet_market_submit_relative_ask(
           const string& from_account,
           const string& quantity,
           const string& quantity_symbol,
           const string& relative_quote_price,
           const string& quote_symbol,
           const string& limit )
{
  auto record = _wallet->submit_relative_ask( from_account, quantity, quantity_symbol, relative_quote_price, quote_symbol, limit, true );
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
                                                                    double amount_to_withdraw )
{
  auto record = _wallet->withdraw_delegate_pay( delegate_name, amount_to_withdraw, to_account_name, true );
  _wallet->cache_transaction( record );
  network_broadcast_transaction( record.trx );
  return record;
}

asset client_impl::wallet_set_transaction_fee( double fee )
{ try {
  oasset_record asset_record = _chain_db->get_asset_record( asset_id_type() );
  FC_ASSERT( asset_record.valid() );
  _wallet->set_transaction_fee( asset( fee * asset_record->precision ) );
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

vote_summary   client_impl::wallet_check_vote_status( const string& account_name )
{
    return _wallet->get_vote_status( account_name );
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


wallet_transaction_record client_impl::wallet_publish_price_feed( const std::string& delegate_account,
                                                                  double real_amount_per_xts,
                                                                  const std::string& real_amount_symbol )
{
   auto record = _wallet->publish_price( delegate_account, real_amount_per_xts, real_amount_symbol, false, true );
   _wallet->cache_transaction( record );
   network_broadcast_transaction( record.trx );
   return record;
}

vector<std::pair<string, wallet_transaction_record>> client_impl::wallet_publish_feeds_multi_experimental( const map<string,double>& real_amount_per_xts )
{
   vector<std::pair<string, wallet_transaction_record>> record_list =
      _wallet->publish_feeds_multi_experimental( real_amount_per_xts, true );

   for( std::pair<string, wallet_transaction_record>& record_pair : record_list )
   {
      wallet_transaction_record& record = record_pair.second;
      _wallet->cache_transaction( record );
      network_broadcast_transaction( record.trx );
   }
   return record_list;
}

wallet_transaction_record client_impl::wallet_publish_feeds( const std::string& delegate_account,
                                                             const map<string,double>& real_amount_per_xts )
{
   auto record = _wallet->publish_feeds( delegate_account, real_amount_per_xts, true );
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

} } } // namespace bts::client::detail
