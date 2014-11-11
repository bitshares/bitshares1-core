#include <bts/client/client.hpp>
#include <bts/client/client_impl.hpp>
#include <bts/utilities/key_conversion.hpp>
#include <bts/wallet/config.hpp>
#include <bts/wallet/exceptions.hpp>

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

wallet_transaction_record detail::client_impl::wallet_publish_slate( const string& publishing_account_name,
                                                                     const string& paying_account_name )
{
   const auto record = _wallet->publish_slate( publishing_account_name, paying_account_name );
   network_broadcast_transaction( record.trx );
   return record;
}

wallet_transaction_record detail::client_impl::wallet_publish_version( const string& publishing_account_name,
                                                                       const string& paying_account_name )
{
   const auto record = _wallet->publish_version( publishing_account_name, paying_account_name );
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
        const vote_selection_method& selection_method )
{
    return wallet_transfer_from(amount_to_transfer, asset_symbol, from_account_name, from_account_name,
                                to_account_name, memo_message, selection_method);
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
    const auto record = _wallet->burn_asset( amount_to_transfer, asset_symbol,
                                             from_account_name, for_or_against, to_account_name,
                                             public_message, anonymous );
    network_broadcast_transaction( record.trx );
    return record;
}


address  detail::client_impl::wallet_create_new_address( const string& account_name, const string& label )
{
    return _wallet->create_new_address( account_name, label );
}


wallet_transaction_record detail::client_impl::wallet_transfer_asset_to_address(
        double amount_to_transfer,
        const string& asset_symbol,
        const string& from_account_name,
        const address& to_address,
        const string& memo_message,
        const vote_selection_method& selection_method )
{
    auto record =  _wallet->transfer_asset_to_address( amount_to_transfer,
                                      asset_symbol,
                                      from_account_name,
                                      to_address,
                                      memo_message,
                                      selection_method,
                                      true);
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
        const vote_selection_method& selection_method )
{
    asset amount = _chain_db->to_ugly_asset(amount_to_transfer, asset_symbol);
    auto sender = _wallet->get_account(from_account_name);
    auto payer = _wallet->get_account(paying_account_name);
    auto recipient = _wallet->get_account(to_account_name);
    transaction_builder_ptr builder = _wallet->create_transaction_builder();
    auto record = builder->deposit_asset(payer, recipient, amount,
                                         memo_message, selection_method, sender.owner_key)
                          .finalize()
                          .sign();

    network_broadcast_transaction( record.trx );
    for( auto&& notice : builder->encrypted_notifications() )
        _mail_client->send_encrypted_message(std::move(notice),
                                             from_account_name,
                                             to_account_name,
                                             recipient.owner_key);

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
        const vote_selection_method& selection_method )
{
    asset amount = _chain_db->to_ugly_asset(amount_to_transfer, asset_symbol);
    auto sender = _wallet->get_account(from_account_name);
    auto payer = _wallet->get_account(paying_account_name);
    auto recipient = _wallet->get_account(to_account_name);
    auto escrow_account = _wallet->get_account(escrow_account_name);
    transaction_builder_ptr builder = _wallet->create_transaction_builder();

    auto record = builder->deposit_asset_with_escrow(payer, recipient, escrow_account, agreement,
                                                     amount, memo_message, selection_method, 
                                                     sender.owner_key)
                          .finalize()
                          .sign();

    network_broadcast_transaction( record.trx );
    for( auto&& notice : builder->encrypted_notifications() )
        _mail_client->send_encrypted_message(std::move(notice),
                                             from_account_name,
                                             to_account_name,
                                             recipient.owner_key);

    return record;
}

wallet_transaction_record detail::client_impl::wallet_asset_create(
        const string& symbol,
        const string& asset_name,
        const string& issuer_name,
        const string& description,
        const variant& data,
        double maximum_share_supply ,
        int64_t precision,
        bool is_market_issued /* = false */ )
{
  const auto record = _wallet->create_asset( symbol, asset_name, description, data, issuer_name,
                                             maximum_share_supply, precision, is_market_issued );
  network_broadcast_transaction( record.trx );
  return record;
}

wallet_transaction_record detail::client_impl::wallet_asset_issue(
        double real_amount,
        const string& symbol,
        const string& to_account_name,
        const string& memo_message )
{
  const auto record = _wallet->issue_asset( real_amount, symbol, to_account_name, memo_message );
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

string detail::client_impl::wallet_import_private_key(const string& wif_key_to_import,
                                       const string& account_name,
                                       bool create_account,
                                       bool wallet_rescan_blockchain)
{
  auto key = _wallet->import_wif_private_key(wif_key_to_import, account_name, create_account );
  if (wallet_rescan_blockchain)
    _wallet->scan_chain(0);

  auto oacct = _wallet->get_account_for_address( address( key ) );
  FC_ASSERT(oacct.valid(), "No account for a key we just imported" );
  _wallet->auto_backup( "key_import" );
  return oacct->name;
}

string detail::client_impl::wallet_dump_private_key( const std::string& input )
{
  try
  {
      ASSERT_TASK_NOT_PREEMPTED(); // make sure no cancel gets swallowed by catch(...)
      //If input is an account name...
      return utilities::key_to_wif( _wallet->get_active_private_key( input ) );
  }
  catch( ... )
  {
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
  }

  return "key not found";
}

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

void client_impl::wallet_rescan_blockchain( uint32_t start, uint32_t count, bool fast_scan )
{ try {
   _wallet->scan_chain( start, start + count, fast_scan );
} FC_RETHROW_EXCEPTIONS( warn, "", ("start",start)("count",count) ) }

wallet_transaction_record client_impl::wallet_scan_transaction( const string& transaction_id, bool overwrite_existing )
{ try {
   return _wallet->scan_transaction( transaction_id, overwrite_existing );
} FC_RETHROW_EXCEPTIONS( warn, "", ("transaction_id",transaction_id)("overwrite_existing",overwrite_existing) ) }

void client_impl::wallet_scan_transaction_experimental( const string& transaction_id, bool overwrite_existing )
{ try {
#ifndef BTS_TEST_NETWORK
   FC_ASSERT( !"This command is for developer testing only!" );
#endif
   _wallet->scan_transaction_experimental( transaction_id, overwrite_existing );
} FC_RETHROW_EXCEPTIONS( warn, "", ("transaction_id",transaction_id)("overwrite_existing",overwrite_existing) ) }

void client_impl::wallet_add_transaction_note_experimental( const string& transaction_id, const string& note )
{ try {
#ifndef BTS_TEST_NETWORK
   FC_ASSERT( !"This command is for developer testing only!" );
#endif
   _wallet->add_transaction_note_experimental( transaction_id, note );
} FC_RETHROW_EXCEPTIONS( warn, "", ("transaction_id",transaction_id)("note",note) ) }

set<pretty_transaction_experimental> client_impl::wallet_transaction_history_experimental( const string& account_name )const
{ try {
#ifndef BTS_TEST_NETWORK
   FC_ASSERT( !"This command is for developer testing only!" );
#endif
   return _wallet->transaction_history_experimental( account_name );
} FC_RETHROW_EXCEPTIONS( warn, "", ("account_name",account_name) ) }

vector<snapshot_record> client_impl::wallet_check_sharedrop()const
{ try {
   return _wallet->check_sharedrop();
} FC_CAPTURE_AND_RETHROW() }

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
    const auto record = _wallet->register_account( account_name, data, delegate_pay_rate, pay_with_account, variant(new_account_type).as<account_type>() );
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
   const auto record = _wallet->update_registered_account( account_to_update, pay_from_account, public_data, delegate_pay_rate );
   network_broadcast_transaction( record.trx );
   return record;
}

wallet_transaction_record detail::client_impl::wallet_account_update_active_key( const std::string& account_to_update,
                                                                                 const std::string& pay_from_account,
                                                                                 const std::string& new_active_key )
{
   const auto record = _wallet->update_active_key( account_to_update, pay_from_account, new_active_key );
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

account_balance_summary_type client_impl::wallet_account_balance( const string& account_name )const
{
  return _wallet->get_account_balances( account_name, false );
}

account_balance_id_summary_type client_impl::wallet_account_balance_ids( const string& account_name )const
{
  return _wallet->get_account_balance_ids( account_name, false );
}

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

  if (!allow_stupid_bid && lowest_ask.size()
      && fc::variant(quote_price).as_double() > _chain_db->to_pretty_price_double(lowest_ask.front().get_price()) * 1.05)
    FC_THROW_EXCEPTION(stupid_order, "You are attempting to bid at more than 5% above the buy price. "
                                     "This bid is based on economically unsound principles, and is ill-advised. "
                                     "If you're sure you want to do this, place your bid again and set allow_stupid_bid to true.");

  const auto record = _wallet->submit_bid( from_account, quantity, quantity_symbol, quote_price, quote_symbol );
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

  const auto record = _wallet->submit_ask( from_account, quantity, quantity_symbol, quote_price, quote_symbol );
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
  const auto record = _wallet->submit_short( from_account,
                                             quantity,
                                             collateral_symbol,
                                             apr,
                                             quote_symbol,
                                             short_price_limit );
  network_broadcast_transaction( record.trx );
  return record;
}

wallet_transaction_record client_impl::wallet_market_batch_update(const std::vector<order_id_type> &cancel_order_ids,
                                                                 const std::vector<order_description> &new_orders,
                                                                 bool sign)
{
  const auto record = _wallet->batch_market_update(cancel_order_ids, new_orders, sign);
  if( sign )
     network_broadcast_transaction( record.trx );
  return record;
}

wallet_transaction_record client_impl::wallet_market_cover(
       const string& from_account,
       const string& quantity,
       const string& quantity_symbol,
       const order_id_type& cover_id )
{
  const auto record = _wallet->cover_short( from_account, quantity, quantity_symbol, cover_id );
  network_broadcast_transaction( record.trx );
  return record;
}

wallet_transaction_record client_impl::wallet_delegate_withdraw_pay( const string& delegate_name,
                                                                    const string& to_account_name,
                                                                    double amount_to_withdraw )
{
  const auto record = _wallet->withdraw_delegate_pay( delegate_name, amount_to_withdraw, to_account_name );
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
   const auto record = _wallet->add_collateral( from_account_name, cover_id, real_quantity_collateral_to_add );
   network_broadcast_transaction( record.trx );
   return record;
}

map<order_id_type, market_order> client_impl::wallet_account_order_list( const string& account_name,
                                                                         uint32_t limit )
{
   return _wallet->get_market_orders( account_name, limit );
}

map<order_id_type, market_order> client_impl::wallet_market_order_list( const string& quote_symbol,
                                                                        const string& base_symbol,
                                                                        uint32_t limit,
                                                                        const string& account_name )
{
   return _wallet->get_market_orders( quote_symbol, base_symbol, limit, account_name );
}

wallet_transaction_record client_impl::wallet_market_cancel_order( const order_id_type& order_id )
{
   const auto record = _wallet->cancel_market_orders( {order_id} );
   network_broadcast_transaction( record.trx );
   return record;
}

wallet_transaction_record client_impl::wallet_market_cancel_orders( const vector<order_id_type>& order_ids )
{
   const auto record = _wallet->cancel_market_orders( order_ids );
   network_broadcast_transaction( record.trx );
   return record;
}

account_vote_summary_type client_impl::wallet_account_vote_summary( const string& account_name )const
{
   if( !account_name.empty() && !_chain_db->is_valid_account_name( account_name ) )
       FC_CAPTURE_AND_THROW( invalid_account_name, (account_name) );

   return _wallet->get_account_vote_summary( account_name );
}

vote_summary   client_impl::wallet_check_vote_proportion( const string& account_name )
{
    return _wallet->get_vote_proportion( account_name );
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

wallet_transaction_record client_impl::wallet_publish_price_feed( const std::string& delegate_account,
                                                                  double real_amount_per_xts,
                                                                  const std::string& real_amount_symbol )
{
   const auto record = _wallet->publish_price( delegate_account, real_amount_per_xts, real_amount_symbol );
   network_broadcast_transaction( record.trx );
   return record;
}
wallet_transaction_record client_impl::wallet_publish_feeds( const std::string& delegate_account,
                                                             const map<string,double>& real_amount_per_xts )
{
   const auto record = _wallet->publish_feeds( delegate_account, real_amount_per_xts );
   network_broadcast_transaction( record.trx );
   return record;
}

void client_impl::wallet_repair_records()
{
   _wallet->auto_backup( "before_record_repair" );
   return _wallet->repair_records();
}

int32_t client_impl::wallet_regenerate_keys( const std::string& account, uint32_t number_to_regenerate )
{
   _wallet->auto_backup( "before_key_regeneration" );
   return _wallet->regenerate_keys( account, number_to_regenerate );
}

} } } // namespace bts::client::detail
