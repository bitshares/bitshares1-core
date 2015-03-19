#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/account_operations.hpp>
#include <bts/blockchain/time.hpp>
#include <bts/client/client.hpp>
#include <bts/client/client_impl.hpp>

#include <fc/thread/non_preemptable_scope_check.hpp>

namespace bts { namespace client { namespace detail {

fc::variant_object client_impl::fetch_welcome_package(const fc::variant_object& arguments)
{
   fc::mutable_variant_object welcome_package;
   welcome_package["chain_id"] = _chain_db->get_chain_id();
   welcome_package["relay_fee_collector"] = _chain_db->get_account_record(_config.relay_account_name);
   welcome_package["network_fee_amount"] = _config.light_network_fee;
   welcome_package["relay_fee_amount"] = _config.light_relay_fee;

   auto all_assets = blockchain_list_assets("", -1);
   if( arguments.contains("last_known_asset_id") )
   {
      asset_id_type last_known_asset_id = arguments["last_known_asset_id"].as<asset_id_type>();
      all_assets.erase(std::remove_if(all_assets.begin(), all_assets.end(), [last_known_asset_id] (const asset_record& rec) {
         return rec.id <= last_known_asset_id;
      }), all_assets.end());
   }
   welcome_package["new_assets"] = all_assets;

   return welcome_package;
}

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
   FC_ASSERT( !_config.faucet_account_name.empty(), "This server isn't a faucet. Cannot register account." );
   if( !_chain_db->is_valid_account_name(account_to_register.name) )
      FC_THROW_EXCEPTION( invalid_account_name, "" );
   if( _chain_db->get_account_record(account_to_register.name) )
      FC_THROW_EXCEPTION( account_already_registered, "" );
   if( _chain_db->get_account_record(account_to_register.owner_key) ||
       _chain_db->get_account_record(account_to_register.active_key()) )
      FC_THROW_EXCEPTION( account_key_in_use, "" );

   transaction_builder_ptr builder = _wallet->create_transaction_builder();
   auto rec = builder->register_account(account_to_register.name, account_to_register.public_data,
                                        account_to_register.owner_key, account_to_register.active_key(),
                                        -1, public_account, _wallet->get_account(_config.faucet_account_name))
         .finalize().sign();
   _wallet->cache_transaction(rec);
   network_broadcast_transaction(rec.trx);

   return true;
} FC_CAPTURE_AND_RETHROW( ) }

bool client_impl::approve_register_account( const string& salt, const string& paying_account_name )
{ try {
   return false;
} FC_CAPTURE_AND_RETHROW( ) }

} } } // namespace bts::client::detail
