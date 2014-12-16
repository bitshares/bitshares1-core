#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/account_operations.hpp>
#include <bts/blockchain/time.hpp>
#include <bts/client/client.hpp>
#include <bts/client/client_impl.hpp>

#include <fc/thread/non_preemptable_scope_check.hpp>

namespace bts { namespace client { namespace detail {

/**
 *  When a light weight client is attempting to register an account for the first time they will
 *  call this method with the account to be registered.  If this light wallet server is
 *  providing a faucet service then this account will be added to a queue.
 *
 *  The public data of the account_to_register must include a unique salt property which
 *  helps the user secure/recover their brain key.
 *
 *  The account name must be globally unique.
 */
bool client_impl::request_register_account( const account_record& account_to_register )
{ try {
   return false;
} FC_CAPTURE_AND_RETHROW( ) }

bool client_impl::approve_register_account( const string& salt, const string& paying_account_name )
{ try {
   return false;
} FC_CAPTURE_AND_RETHROW( ) }

vector<account_record> client_impl::get_registration_requests()
{
   return vector<account_record>();
}


} } } // namespace bts::client::detail
