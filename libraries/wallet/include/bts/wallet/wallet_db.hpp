#pragma once
#include <bts/db/level_map.hpp>
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

         int32_t              new_wallet_record_index();
         int32_t              new_key_child_index();
         fc::ecc::private_key new_private_key( const fc::sha512& password, 
                                               const address& parent_account_address = address() );

         void        set_property( property_enum property_id, const fc::variant& v );
         fc::variant get_property( property_enum property_id );

         void store_key( const key_data& k );
         void store_transaction( wallet_transaction_record& t );
         void cache_balance( const bts::blockchain::balance_record& b );
         void cache_balance( const wallet_balance_record& b );
         void cache_account( const wallet_account_record& );
         void cache_memo( const memo_status& memo, 
                          const fc::ecc::private_key& account_key,
                          const fc::sha512& password );
         void cache_order( const bts::blockchain::market_order& );
         void remove_balance( const balance_id_type& balance_id );

         void clear_pending_transactions();

         void store_balance( const wallet_balance_record& r )
         {
            store_record( r );
            balances[r.id()] = r;
         }
         void update_market_order( const address& owner, 
                                   optional<bts::blockchain::market_order>& order,
                                   const transaction_id_type& trx_id );

         wallet_transaction_record  cache_transaction( const signed_transaction& trx,
                                 const asset&           amount, share_type fees,
                                 const string&          memo_message,
                                 const public_key_type& to,
                                 time_point_sec         received = time_point_sec(),
                                 time_point_sec         created = time_point_sec(),
                                 public_key_type        from = public_key_type(),
                                 const vector<address>& extra_addresses = vector<address>()
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
         owallet_account_record lookup_account( account_id_type aid )const;

         oprivate_key           lookup_private_key( const address& address, 
                                                    const fc::sha512& password );

         owallet_balance_record lookup_balance( const balance_id_type& balance_id );
         owallet_key_record     lookup_key( const address& address )const;


         owallet_setting_record   lookup_setting(const string& name)const;
         void                     store_setting(const string& name, const variant& value);

         bool has_private_key( const address& a )const;

         void add_account( const string& new_account_name, 
                           const public_key_type& new_account_key,
                           const variant& private_data = variant() );
         void add_account( const account_record& blockchain_account,
                           const variant& private_data = variant() );

         void remove_contact_account( const string& account_name);

         void rename_account( const string& old_account_name,
                              const string& new_account_name );

         void export_records(std::vector<generic_wallet_record>& records) const;
         void import_records(const std::vector<generic_wallet_record>& records);

         bool                           validate_password( const fc::sha512& password )const;
         optional<extended_private_key> get_master_key( const fc::sha512& password    )const;
         void                           set_master_key( const extended_private_key& key, 
                                                        const fc::sha512& new_password );

         void                           change_password( const fc::sha512& old_password,
                                                         const fc::sha512& new_password );

         const unordered_map< transaction_id_type, wallet_transaction_record >&  get_transactions()const
         {
            return transactions;
         }
         const unordered_map< balance_id_type,wallet_balance_record >&  get_balances()const
         {
            return balances;
         }
         const unordered_map< int32_t,wallet_account_record >& get_accounts()const
         {
            return accounts;
         }
         const unordered_map<address,wallet_market_order_status_record>&  get_market_orders()const
         {
            return market_orders;
         }
         const unordered_map< address, wallet_key_record >& get_keys()const 
         {
            return keys;
         }

      private:
         optional<wallet_master_key_record>                               wallet_master_key;
 
         /** maps wallet_record_index to accounts */
         unordered_map< int32_t,wallet_account_record >                   accounts;
         unordered_map< address, wallet_key_record >                      keys;
         unordered_map< transaction_id_type, wallet_transaction_record >  transactions;
         map<string,wallet_asset_record>                                  assets;
         unordered_map< balance_id_type,wallet_balance_record >           balances;
         map<property_enum, wallet_property_record>                       properties;
         unordered_map<address,wallet_market_order_status_record>         market_orders;
         map< string, wallet_setting_record >                             settings;

         unordered_map< address, address >                                btc_to_bts_address;
         unordered_map< address, int32_t >                                address_to_account_wallet_record_index;
         unordered_map< account_id_type, int32_t >                        account_id_to_wallet_record_index;
         map< string, int32_t >                                           name_to_account_wallet_record_index;

         void remove_item( int32_t index );
         /**
          *  This is private 
          */
         template<typename T>
         void store_record( T record_to_store )
         {
            if( record_to_store.wallet_record_index == 0 ) 
               record_to_store.wallet_record_index = new_wallet_record_index();
            store_generic_record( record_to_store.wallet_record_index, generic_wallet_record( record_to_store ) );
         }

        void store_generic_record( int32_t index, const generic_wallet_record& r );

        friend class detail::wallet_db_impl;
        unique_ptr<detail::wallet_db_impl> my;
   };

} } // bts::wallet


