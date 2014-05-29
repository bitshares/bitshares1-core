#pragma once
#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/extended_address.hpp>
#include <bts/blockchain/withdraw_types.hpp>
#include <fc/reflect/variant.hpp>
#include <fc/io/json.hpp>
#include <fc/io/raw_variant.hpp>
#include <fc/log/logger.hpp>

#include <bts/wallet/wallet_records.hpp>

namespace bts { namespace wallet {

   typedef std::vector<fc::ecc::private_key> private_keys;
   using std::string;

   namespace detail { class wallet_db_impl; }

   class wallet_db
   {
      public:
         wallet_db();
         ~wallet_db();
         
         void open( const fc::path& wallet_file );
         void close();
         
         bool is_open()const;

         template<typename T>
         void store_record( T t )
         {
            if( t.index == 0 ) 
               t.index = new_index();
            ilog( "Storing: ${rec}", ("rec",t) );
            store_generic_record( t.index, generic_wallet_record( t ) );
         }
         int32_t              new_index();
         int32_t              new_key_child_index();
         fc::ecc::private_key new_private_key( const fc::sha512& password, 
                                               const address& parent_account_address = address() );

         void        set_property( property_enum property_id, const fc::variant& v );
         fc::variant get_property( property_enum property_id );

         void store_key( const key_data& k );
         void store_transaction( const signed_transaction& t );
         void cache_balance( const bts::blockchain::balance_record& b );
         void cache_memo( const memo_status& memo, 
                          const fc::ecc::private_key& account_key,
                          const fc::sha512& password );

         void cache_transaction( const signed_transaction& trx,
                                 const string& memo_message,
                                 const public_key_type& to );

         private_keys get_account_private_keys( const fc::sha512& password );

         owallet_account_record lookup_account( const address& address_of_public_key );
         owallet_account_record lookup_account( const string& account_name );
         fc::optional<fc::ecc::private_key>  lookup_private_key( const address& address, 
                                                                 const fc::sha512& password );
         owallet_balance_record lookup_balance( const balance_id_type& balance_id );
         owallet_key_record     lookup_key( const address& address );

         bool has_private_key( const address& a )const;

         void add_contact_account( const string& new_account_name, 
                                   const public_key_type& new_account_key );

         void rename_account( const string& old_account_name,
                              const string& new_account_name );


         fc::optional<wallet_master_key_record>                 wallet_master_key;

         std::unordered_map<address,wallet_key_record>          keys;
         std::unordered_map<address,address>                    btc_to_bts_address;
         std::unordered_map<address,int32_t>                    address_to_account;
         std::unordered_map<name_id_type,int32_t>               name_id_to_account;
         std::map<string,int32_t>                          name_to_account;

         std::unordered_map< int32_t,wallet_account_record >                   accounts;
         std::unordered_map< transaction_id_type, wallet_transaction_record >  transactions;
         std::unordered_map< balance_id_type,wallet_balance_record >           balances;
         std::map<string,wallet_name_record>                              names;
         std::map<string,wallet_asset_record>                             assets;
         std::map<property_enum, wallet_property_record>                       properties;

         void export_to_json( const fc::path& export_file_name ) const;

         void create_from_json( const fc::path& file_to_import, 
                                const fc::path& wallet_to_create );

      private:
        void store_generic_record( int32_t index, const generic_wallet_record& r );
        std::unique_ptr<detail::wallet_db_impl> my;
   };

} } // bts::wallet


