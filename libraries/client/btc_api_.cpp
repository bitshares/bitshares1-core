#include <bts/client/client.hpp>
#include <bts/client/client_impl.hpp>
#include <bts/utilities/key_conversion.hpp>


namespace bts { namespace client { namespace detail {

string   detail::client_impl::btc_dumpprivkey( const string& real_account,
                                              const string& asset_symbol,
                                              const address& addr ) const
{ try {
    return utilities::key_to_wif( _wallet->get_private_key( addr ) );
} FC_RETHROW_EXCEPTIONS( warn, "" ) }

string   detail::client_impl::btc_getaccount( const string& real_account,
                                              const string& asset_symbol,
                                              const address& addr ) const
{ try {
    return _wallet->get_address_group_label( addr );
} FC_RETHROW_EXCEPTIONS( warn, "" ) }

address   detail::client_impl::btc_getaccountaddress( const string& real_account,
                                                      const string& asset_symbol,
                                                      const string& account ) const
{ try {
    // Warning: This doesn't behave like BTC
    return btc_getnewaddress( real_account, asset_symbol, account );
} FC_RETHROW_EXCEPTIONS( warn, "" ) }

vector<address>   detail::client_impl::btc_getaddressesbyaccount( const string& real_account,
                                                                  const string& asset_symbol,
                                                                  const string& virtual_account ) const
{ try {
    return _wallet->get_addresses_for_group_label( virtual_account );
} FC_RETHROW_EXCEPTIONS( warn, "" ) }

double   detail::client_impl::btc_getbalance( const string& real_account,
                                              const string& asset_symbol,
                                              const string& virtual_account ) const
{ try {
    auto asset_rec = _chain_db->get_asset_record( asset_symbol );
    FC_ASSERT( asset_rec.valid(), "No such asset." );
    auto account_balances = _wallet->get_account_balance_records( real_account );
    auto total = asset( 0, asset_rec->id );
    auto balances = account_balances[real_account];
    for( const auto& bal : balances )
    {
        if( bal.asset_id() != asset_rec->id )
            continue;
        auto addr = bal.owner();
        auto virtual_acct = _wallet->get_address_group_label( addr );
        if( virtual_acct == virtual_account )
            total += bal.get_spendable_balance( _chain_db->now() );
    }
    return double(total.amount) / double(asset_rec->precision);
} FC_RETHROW_EXCEPTIONS( warn, "" ) }

address   detail::client_impl::btc_getnewaddress( const string& real_account,
                                                  const string& asset_symbol,
                                                  const string& account ) const
{ try {
    return _wallet->create_new_address( real_account, "" );
} FC_RETHROW_EXCEPTIONS( warn, "" ) }

address   detail::client_impl::btc_importprivkey( const string& real_account,
                                                  const string& asset_symbol,
                                                  const string& wif_privkey,
                                                  const string& label,
                                                  bool rescan ) const
{ try {
    auto pubkey = _wallet->import_wif_private_key( wif_privkey, real_account );
    auto addr = address(pubkey);
    _wallet->set_address_label( addr, label );
    return addr;
} FC_RETHROW_EXCEPTIONS( warn, "" ) }


std::vector<string>   detail::client_impl::btc_listaccounts( const string& real_account,
                                                             const string& asset_symbol ) const
{ try {
    auto pubkeys = _wallet->get_public_keys_in_account( real_account );
    auto accounts = std::set<string>();
    for( const auto& key : pubkeys )
    {
        accounts.insert( _wallet->get_address_group_label( address(key) ) );
    }
    return std::vector<string>(accounts.begin(), accounts.end());
} FC_RETHROW_EXCEPTIONS( warn, "" ) }

std::map<string, double>  detail::client_impl::btc_listaddressgroupings( const string& real_account,
                                                                         const string& asset_symbol ) const
{ try {
    auto pubkeys = _wallet->get_public_keys_in_account( real_account );
    auto ret = map<string, double>();
    for( const auto& key : pubkeys )
    {
        FC_ASSERT(!"unimplemented");
    }
    return ret;
} FC_RETHROW_EXCEPTIONS( warn, "" ) }


}}} // bts::client::detail
