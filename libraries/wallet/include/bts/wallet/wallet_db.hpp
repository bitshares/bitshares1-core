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

   typedef vector<fc::ecc::private_key> private_keys;

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
            store_generic_record( t.index, generic_wallet_record( t ) );
         }
         int32_t              new_index();
         int32_t              new_key_child_index();
         fc::ecc::private_key new_private_key( const fc::sha512& password, 
                                               const address& parent_account_address = address() );

         void        set_property( property_enum property_id, const fc::variant& v );
         fc::variant get_property( property_enum property_id );

         void store_key( const key_data& k );
         void store_transaction( wallet_transaction_record& t );
         void cache_balance( const bts::blockchain::balance_record& b );
         void cache_account( const wallet_account_record& );
         void cache_memo( const memo_status& memo, 
                          const fc::ecc::private_key& account_key,
                          const fc::sha512& password );

         void store_balance( const wallet_balance_record& r )
         {
            store_record( r );
            balances[r.id()] = r;
         }

         wallet_transaction_record  cache_transaction( const signed_transaction& trx,
                                 const asset& amount, share_type fees,
                                 const string& memo_message,
                                 const public_key_type& to,
                                 time_point_sec received = time_point_sec(),
                                 time_point_sec created = time_point_sec(),
                                 public_key_type from = public_key_type()
                                 );

         owallet_transaction_record lookup_transaction( const transaction_id_type& trx_id )const
         {
            auto itr = transactions.find(trx_id);
            if( itr != transactions.end() ) return itr->second;
            return owallet_transaction_record();
         }

         private_keys get_account_private_keys( const fc::sha512& password );
         string       get_account_name( const address& account_address )const;

         owallet_account_record lookup_account( const address& address_of_public_key )const;
         owallet_account_record lookup_account( const string& account_name )const;

         oprivate_key           lookup_private_key( const address& address, 
                                                    const fc::sha512& password );

         owallet_balance_record lookup_balance( const balance_id_type& balance_id );
         owallet_key_record     lookup_key( const address& address )const;

         bool has_private_key( const address& a )const;

         void add_contact_account( const string& new_account_name, 
                                   const public_key_type& new_account_key,
                                   const variant& private_data = variant() );
         void add_contact_account( const account_record& blockchain_account,
                                   const variant& private_data = variant() );

         void rename_account( const string& old_account_name,
                              const string& new_account_name );

         void export_to_json( const fc::path& export_file_name ) const;

         void create_from_json( const fc::path& file_to_import, 
                                const fc::path& wallet_to_create );

         fc::optional<wallet_master_key_record>                 wallet_master_key;

         unordered_map<address,wallet_key_record>                         keys;
         unordered_map<address,address>                                   btc_to_bts_address;
         unordered_map<address,int32_t>                                   address_to_account;
         unordered_map<account_id_type,int32_t>                           account_id_to_account;
         map<string,int32_t>                                              name_to_account;

         unordered_map< int32_t,wallet_account_record >                   accounts;
         unordered_map< transaction_id_type, wallet_transaction_record >  transactions;
         unordered_map< balance_id_type,wallet_balance_record >           balances;
         map<string,wallet_asset_record>                                  assets;
         map<property_enum, wallet_property_record>                       properties;

      private:
        void store_generic_record( int32_t index, const generic_wallet_record& r );
        unique_ptr<detail::wallet_db_impl> my;
   };

} } // bts::wallet


