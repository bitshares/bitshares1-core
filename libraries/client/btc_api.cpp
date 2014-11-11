#include <bts/client/client.hpp>
#include <bts/client/client_impl.hpp>


namespace bts { namespace client { namespace detail {

string   detail::client_impl::btc_dumpprivkey( const string& real_account,
                                              const string& asset_symbol,
                                              const address& addr ) const
{ try {

} FC_RETHROW_EXCEPTIONS( warn, "" ) }

string   detail::client_impl::btc_getaccount( const string& real_account,
                                              const string& asset_symbol,
                                              const address& addr ) const
{ try {
} FC_RETHROW_EXCEPTIONS( warn, "" ) }

address   detail::client_impl::btc_getaccountaddress( const string& real_account,
                                                      const string& asset_symbol,
                                                      const string& account ) const
{ try {

} FC_RETHROW_EXCEPTIONS( warn, "" ) }

vector<address>   detail::client_impl::btc_getaddressesbyaccount( const string& real_account,
                                                                  const string& asset_symbol,
                                                                  const string& account ) const
{ try {

} FC_RETHROW_EXCEPTIONS( warn, "" ) }

double   detail::client_impl::btc_getbalance( const string& real_account,
                                              const string& asset_symbol,
                                              const string& account ) const
{ try {

} FC_RETHROW_EXCEPTIONS( warn, "" ) }






}}} // bts::client::detail
