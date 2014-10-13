#include <bts/wallet/url.hpp>
#include <bts/wallet/wallet.hpp>
#include <bts/wallet/wallet_impl.hpp>

using namespace bts::wallet;
using namespace bts::wallet::detail;

void wallet_impl::login_map_cleaner_task()
{
  std::vector<public_key_type> expired_records;
  for( const auto& record : _login_map )
    if( fc::time_point::now() - record.second.insertion_time >= fc::seconds(_login_lifetime_seconds) )
      expired_records.push_back(record.first);
  ilog("Purging ${count} expired records from login map.", ("count", expired_records.size()));
  for( const auto& record : expired_records )
    _login_map.erase(record);

  if( !_login_map.empty() )
    _login_map_cleaner_done = fc::schedule([this](){ login_map_cleaner_task(); },
                                           fc::time_point::now() + fc::seconds(_login_cleaner_interval_seconds),
                                           "login_map_cleaner_task");
}

std::string wallet::login_start(const std::string& account_name)
{ try {
   FC_ASSERT( is_open() );
   FC_ASSERT( is_unlocked() );

   auto key = my->_wallet_db.lookup_key( get_account(account_name).active_address() );
   FC_ASSERT( key.valid() );
   FC_ASSERT( key->has_private_key() );

   private_key_type one_time_key = private_key_type::generate();
   public_key_type one_time_public_key = one_time_key.get_public_key();
   my->_login_map[one_time_public_key] = {one_time_key, fc::time_point::now()};

   if( !my->_login_map_cleaner_done.valid() || my->_login_map_cleaner_done.ready() )
     my->_login_map_cleaner_done = fc::schedule([this](){ my->login_map_cleaner_task(); },
                                                fc::time_point::now() + fc::seconds(my->_login_cleaner_interval_seconds),
                                                "login_map_cleaner_task");

   auto signature = key->decrypt_private_key(my->_wallet_password)
                       .sign_compact(fc::sha256::hash((char*)&one_time_public_key,
                                                      sizeof(one_time_public_key)));

   return CUSTOM_URL_SCHEME ":Login/" + variant(public_key_type(one_time_public_key)).as_string()
                                      + "/" + fc::variant(signature).as_string() + "/";
} FC_RETHROW_EXCEPTIONS( warn, "", ("account_name",account_name) ) }

fc::variant wallet::login_finish(const public_key_type& server_key,
                                 const public_key_type& client_key,
                                 const fc::ecc::compact_signature& client_signature)
{ try {
   FC_ASSERT( is_open() );
   FC_ASSERT( is_unlocked() );
   FC_ASSERT( my->_login_map.find(server_key) != my->_login_map.end(), "Login session has expired. Generate a new login URL and try again." );

   private_key_type private_key = my->_login_map[server_key].key;
   my->_login_map.erase(server_key);
   auto secret = private_key.get_shared_secret( fc::ecc::public_key_data(client_key) );
   auto user_account_key = fc::ecc::public_key(client_signature, fc::sha256::hash(secret.data(), sizeof(secret)));

   fc::mutable_variant_object result;
   result["user_account_key"] = public_key_type(user_account_key);
   result["shared_secret"] = secret;
   return result;
} FC_RETHROW_EXCEPTIONS( warn, "", ("server_key",server_key)("client_key",client_key)("client_signature",client_signature) ) }
