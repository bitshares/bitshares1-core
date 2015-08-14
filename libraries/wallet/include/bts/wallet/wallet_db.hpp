#pragma once

#include <bts/wallet/wallet_records.hpp>

namespace bts { namespace wallet {

   namespace detail { class wallet_db_impl; }

   struct exported_account_keys
   {
       string account_name;
       vector<vector<char>> encrypted_private_keys;
       vector<public_key_type> public_keys;
   };

   struct exported_keys
   {
       fc::sha512 password_checksum;
       vector<exported_account_keys> account_keys;
   };

   class wallet_db
   {
      public:
         wallet_db();
         ~wallet_db();

         void open( const fc::path& wallet_file );
         void close();
         bool is_open()const;

         int32_t new_wallet_record_index();

         void        set_property( property_enum property_id, const fc::variant& v );
         fc::variant get_property( property_enum property_id )const;

         // ********************************************************************
         // Most recently rewritten by Vikram

         // Wallet child keys
         uint32_t               get_last_wallet_child_key_index()const;
         void                   set_last_wallet_child_key_index( uint32_t key_index );
         private_key_type       get_wallet_child_key( const fc::sha512& password, uint32_t key_index )const;
         public_key_type        generate_new_account( const fc::sha512& password, const string& account_name );
         public_key_type        generate_new_account( const fc::sha512& password, const string& account_name,
                                                      const private_key_type& owner_private_key );

         // Account child keys
         private_key_type       get_account_child_key( const private_key_type& parent_private_key, uint32_t child_key_index )const;
         private_key_type       get_account_child_key_v1( const fc::sha512& password, const address& account_address,
                                                          uint32_t child_key_index )const;
         private_key_type       generate_new_account_child_key( const fc::sha512& password, const string& account_name,
                                                                const account_key_type parent_key_type = account_key_type::active_key );

         // Account getters and setters
         owallet_account_record lookup_account( const address& account_address )const;
         owallet_account_record lookup_account( const string& account_name )const;
         owallet_account_record lookup_account( const account_id_type account_id )const;
         wallet_account_record  store_account( const account_data& account );
         wallet_account_record  store_account( const blockchain::account_record& blockchain_account_record );
         void                   remove_account( const wallet_account_record& record );

         // Key getters and setters
         owallet_key_record     lookup_key( const address& derived_address )const;
         void                   store_key( const key_data& key );
         void                   import_key( const fc::sha512& password, const string& account_name,
                                            const private_key_type& private_key, bool move_existing );

         // Contact getters and setters
         owallet_contact_record lookup_contact( const variant& data )const;
         owallet_contact_record lookup_contact( const string& label )const;
         wallet_contact_record  store_contact( const contact_data& contact );
         owallet_contact_record remove_contact( const variant& data );
         owallet_contact_record remove_contact( const string& label );

         // Approval getters and setters
         owallet_approval_record lookup_approval( const string& name )const;
         wallet_approval_record  store_approval( const approval_data& approval );
         owallet_approval_record remove_approval( const string& name );

         // Transaction getters and setters
         owallet_transaction_record lookup_transaction( const transaction_id_type& id )const;
         void store_transaction( const transaction_data& transaction );

         // Non-deterministic and not linked to any account
         private_key_type       generate_new_one_time_key( const fc::sha512& password );

         map<private_key_type, string> get_account_private_keys( const fc::sha512& password )const;

         // Restore as many broken record invariants as possible
         void                   repair_records( const fc::sha512& password );
         // ********************************************************************

         void cache_memo( const memo_status& memo,
                          const private_key_type& account_key,
                          const fc::sha512& password );

         void remove_transaction( const transaction_id_type& record_id );

         vector<wallet_transaction_record> get_pending_transactions()const;

         string                        get_account_name( const address& account_address )const;

         owallet_setting_record   lookup_setting(const string& name)const;
         void                     store_setting(const string& name, const variant& value);

         bool has_private_key( const address& a )const;

         void rename_account( const public_key_type& old_account_key,
                              const string& new_account_name );

         void export_to_json( const path& filename )const;
         void import_from_json( const path& filename );

         void export_keys( const path& filename )const;

         bool validate_password( const fc::sha512& password )const;

         void set_master_key( const extended_private_key& key,
                              const fc::sha512& new_password );

         void change_password( const fc::sha512& old_password,
                               const fc::sha512& new_password );

         const unordered_map<transaction_id_type, wallet_transaction_record>& get_transactions()const { return transactions; }
         const unordered_map<int32_t,wallet_account_record>& get_accounts()const { return accounts; }
         const unordered_map<address, wallet_key_record>& get_keys()const { return keys; }
         const unordered_map<string, wallet_contact_record>& get_contacts()const { return contacts; }
         const unordered_map<string, wallet_approval_record>& get_approvals()const { return approvals; }

         unordered_map<transaction_id_type, transaction_ledger_entry>   experimental_transactions;

      private:
         map<property_enum, wallet_property_record>                     properties;
         optional<wallet_master_key_record>                             wallet_master_key;
         unordered_map<int32_t, wallet_account_record>                  accounts;
         unordered_map<address, wallet_key_record>                      keys;
         unordered_map<string, wallet_contact_record>                   contacts;
         unordered_map<string, wallet_approval_record>                  approvals;
         unordered_map<transaction_id_type, wallet_transaction_record>  transactions;
         unordered_map<string, wallet_setting_record>                   settings;

         // Caches to lookup accounts
         unordered_map<address, int32_t>                                address_to_account_wallet_record_index;
         unordered_map<string, int32_t>                                 name_to_account_wallet_record_index;
         unordered_map<account_id_type, int32_t>                        account_id_to_wallet_record_index;

         // Cache to lookup keys
         unordered_map<address, address>                                btc_to_bts_address;

         // Cache to lookup accounts and contacts
         unordered_map<string, string>                                  label_to_account_or_contact;

         // Cache to lookup transactions
         unordered_map<transaction_id_type, transaction_id_type>        id_to_transaction_record_index;

         void remove_item( int32_t index );

         template<typename T>
         void store_and_reload_record( T& record_to_store, const bool sync = false )
         {
            if( record_to_store.wallet_record_index == 0 )
               record_to_store.wallet_record_index = new_wallet_record_index();
            store_and_reload_generic_record( generic_wallet_record( record_to_store ), sync );
         }

         void store_and_reload_generic_record( const generic_wallet_record& record, const bool sync = false );

         friend class detail::wallet_db_impl;
         unique_ptr<detail::wallet_db_impl> my;
   };

} } // bts::wallet

FC_REFLECT( bts::wallet::exported_account_keys,
        (account_name)
        (encrypted_private_keys)
        (public_keys)
        )

FC_REFLECT( bts::wallet::exported_keys,
        (password_checksum)
        (account_keys)
        )
