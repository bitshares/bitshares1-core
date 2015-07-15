#pragma once

#include <bts/blockchain/asset_operations.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/blockchain/transaction_creation_state.hpp>
#include <bts/wallet/pretty.hpp>
#include <bts/wallet/transaction_builder.hpp>

#include <fc/signals.hpp>

namespace bts { namespace wallet {

   using namespace bts::blockchain;

   namespace detail { class wallet_impl; }

   typedef map<string, vector<balance_record>> account_balance_record_summary_type;
   typedef map<string, unordered_set<balance_id_type>> account_balance_id_summary_type;
   typedef map<string, map<asset_id_type, share_type>> account_balance_summary_type;
   typedef map<string, map<string, vector<asset>>> account_extended_balance_type;

   typedef map<string, vector<pretty_vesting_balance>> account_vesting_balance_summary_type;

   typedef map<string, int64_t> account_vote_summary_type;

   typedef std::pair<order_type_enum, vector<string>> order_description;

   enum delegate_status_flags
   {
       any_delegate_status      = 0,
       enabled_delegate_status  = 1 << 0,
       active_delegate_status   = 1 << 1,
       disabled_delegate_status = 1 << 2,
       inactive_delegate_status = 1 << 3
   };

   class wallet
   {
      public:
         wallet( chain_database_ptr chain, bool enabled = true );
         virtual ~wallet();

         void initialize_transaction_creator( transaction_creation_state& c, const string& account_name );
         void sign_transaction_creator( transaction_creation_state& c );

         //Emitted when wallet claims a new transaction. Argument is new ledger entry.
         fc::signal<void( ledger_entry )> wallet_claimed_transaction;

         /**
          *  Wallet File Management
          */
         ///@{
         void    set_data_directory( const path& data_dir );
         path    get_data_directory()const;

         void    create( const string& wallet_name,
                         const string& password,
                         const string& brainkey = string() );
         void    open( const string& wallet_name );
         void    close();

         bool    is_enabled()const;
         bool    is_open()const;
         string  get_wallet_name()const;

         void    export_to_json( const path& filename )const;
         void    create_from_json( const path& filename, const string& wallet_name, const string& passphrase );

         void    export_keys( const path& filename )const;

         void    auto_backup( const string& reason )const;

         void    write_latest_builder( const transaction_builder& builder,
                                       const string& alternate_path );
         ///@}

         /**
          *  Properties
          */
         ///@{

         void                   set_version( uint32_t v );
         uint32_t               get_version()const;

         void                   set_automatic_backups( bool enabled );
         bool                   get_automatic_backups()const;

         void                   set_transaction_scanning( bool enabled );
         bool                   get_transaction_scanning()const;

         void                   set_last_scanned_block_number( uint32_t block_num );
         uint32_t               get_last_scanned_block_number()const;

         void                   set_transaction_fee( const asset& fee );
         asset                  get_transaction_fee( const asset_id_type desired_fee_asset_id = 0 )const;
         bool                   asset_can_pay_network_fee( const asset_id_type desired_fee_asset_id = 0 )const;

         void                   set_transaction_expiration( uint32_t secs );
         uint32_t               get_transaction_expiration()const;

         float                  get_scan_progress()const;

         void                   set_setting( const string& name, const variant& value );
         fc::optional<variant>  get_setting( const string& name )const;

         ///@}

         /**
          *  Lock management & security
          */
         ///@{
         void                               lock();
         void                               unlock( const string& password, uint32_t timeout_seconds );

         bool                               is_locked()const;
         bool                               is_unlocked()const;
         fc::optional<fc::time_point_sec>   unlocked_until()const;

         void                               change_passphrase(const string& new_passphrase);
         ///@}

         /**
          *  @name Utility Methods
          */
         ///@{
         private_key_type           get_active_private_key( const string& account_name )const;
         public_key_type            get_active_public_key( const string& account_name )const;
         public_key_type            get_owner_public_key( const string& account_name )const;

         public_key_summary         get_public_key_summary( const public_key_type& pubkey ) const;
         vector<public_key_type>    get_public_keys_in_account( const string& account_name )const;
         ///@}

         wallet_transaction_record get_transaction( const string& transaction_id_prefix )const;

         vector<wallet_transaction_record>          get_pending_transactions()const;
         map<transaction_id_type, fc::exception>    get_pending_transaction_errors()const;

         void start_scan( const uint32_t start_block_num, const uint32_t limit, const bool async = true );
         void cancel_scan();

         wallet_transaction_record         scan_transaction( const string& transaction_id_prefix, bool overwrite_existing );
         transaction_ledger_entry          scan_transaction_experimental( const string& transaction_id_prefix, bool overwrite_existing );

         void add_transaction_note_experimental( const string& transaction_id_prefix, const string& note );
         set<pretty_transaction_experimental> transaction_history_experimental( const string& account_name );
         pretty_transaction_experimental to_pretty_transaction_experimental( const transaction_ledger_entry& record );

         vector<wallet_transaction_record> get_transactions( const string& transaction_id_prefix );

         vector<wallet_account_record> list_accounts()const;
         owallet_account_record lookup_account( const string& account )const;
         wallet_account_record store_account( const account_data& account );

         public_key_type create_account( const string& account_name );
         void rename_account( const string& old_contact_name,
                              const string& new_contact_name );

         wallet_account_record get_account( const string& account_name )const;
         owallet_account_record  get_account_for_address( address addr )const;

         vector<wallet_contact_record> list_contacts()const;
         owallet_contact_record lookup_contact( const variant& data )const;
         owallet_contact_record lookup_contact( const string& label )const;
         wallet_contact_record store_contact( const contact_data& contact );
         owallet_contact_record remove_contact( const variant& data );
         owallet_contact_record remove_contact( const string& label );

         vector<wallet_approval_record> list_approvals()const;
         owallet_approval_record lookup_approval( const string& name )const;
         wallet_approval_record store_approval( const approval_data& approval );

         variant get_info()const;

         /**
          *  Block Generation API
          */
         ///@{
         void set_delegate_block_production( const string& delegate_id, bool enabled = true );

         ///@param delegates_to_retrieve Type is delegate_status_flags. Uses int type to allow ORing multiple flags
         vector<wallet_account_record> get_my_delegates( uint32_t delegates_to_retrieve = any_delegate_status )const;

         optional<time_point_sec> get_next_producible_block_timestamp( const vector<wallet_account_record>& delegate_records )const;

         /** sign a block if this wallet controls the key for the active delegate, or throw */
         void sign_block( signed_block_header& header )const;
         ///@}

         fc::ecc::compact_signature  sign_hash(const string& signer, const fc::sha256& hash )const;

         /**
          *  Account management API
          */
         ///@{
         vector<string> list() const; // list wallet directories

         uint32_t           import_bitcoin_wallet( const path& wallet_dat,
                                                   const string& wallet_dat_passphrase,
                                                   const string& account_name );

         uint32_t           import_electrum_wallet( const path& wallet_dat,
                                                    const string& wallet_dat_passphrase,
                                                    const string& account_name );

         void               import_keyhotee( const string& firstname,
                                             const string& middlename,
                                             const string& lastname,
                                             const string& brainkey,
                                             const string& keyhoteeid );

         bool               friendly_import_private_key( const private_key_type& key, const string& account_name );
         public_key_type    import_private_key( const private_key_type& new_private_key,
                                                const optional<string>& account_name,
                                                bool create_account = false );

         public_key_type    import_wif_private_key( const string& wif_key,
                                                    const optional<string>& account_name,
                                                    bool create_account = false );

         public_key_type    get_new_public_key( const string& account_name );

         ///@}

         /**
          *  Transaction Generation Methods
          */
         ///@{

         std::shared_ptr<transaction_builder> create_transaction_builder();
         std::shared_ptr<transaction_builder> create_transaction_builder(const transaction_builder& old_builder);
         std::shared_ptr<transaction_builder> create_transaction_builder_from_file(const string& old_builder_path = "");

         void cache_transaction( wallet_transaction_record& transaction_record );

         wallet_transaction_record transfer(
                 const asset& amount,
                 const string& sender_account_name,
                 const string& generic_recipient,
                 const string& memo,
                 const vote_strategy strategy,
                 bool sign
                 );
         wallet_transaction_record burn_asset(
                 const asset& asset_to_transfer,
                 const string& paying_account_name,
                 const string& for_or_against,
                 const string& to_account_name,
                 const string& public_message,
                 bool anonymous,
                 bool sign
                 );
         /**
          * if the active_key is null then the active key will be made the same as the master key.
          * if the name already exists then it will be updated if this wallet controls the active key
          * or master key
          */
         wallet_transaction_record register_account(
                 const string& account_name,
                 const variant& json_data,
                 uint8_t delegate_pay_rate,
                 const string& pay_with_account_name,
                 bts::blockchain::account_type new_account_type,
                 bool sign
                 );
         wallet_transaction_record update_registered_account(
                 const string& account_name,
                 const string& pay_from_account,
                 optional<variant> public_data,
                 uint8_t delegate_pay_rate,
                 bool sign
                 );
         wallet_transaction_record update_active_key(
                 const std::string& account_to_update,
                 const std::string& pay_from_account,
                 const std::string& new_active_key,
                 bool sign
                 );
         wallet_transaction_record retract_account(
                 const std::string& account_to_retract,
                 const std::string& pay_from_account,
                 bool sign
                 );
         wallet_transaction_record withdraw_delegate_pay(
                 const string& delegate_name,
                 const asset& amount,
                 const string& withdraw_to_account_name,
                 bool sign
                 );
         wallet_transaction_record publish_feeds(
                 const string& account,
                 map<string,string> amount_per_xts,
                 bool sign
                 );
         vector<std::pair<string, wallet_transaction_record>>
                 publish_feeds_multi_experimental(
                 map<string,string> amount_per_xts,
                 bool sign
                 );
         wallet_transaction_record publish_price(
                 const string& account,
                 const price& new_price,
                 bool sign
                 );
         transaction_builder set_vote_info(
                 const balance_id_type& balance_id,
                 const address& voter_address,
                 vote_strategy selection_method
                 );
         wallet_transaction_record publish_slate(
                 const string& account_to_publish_under,
                 const string& account_to_pay_with,
                 bool sign
                 );
         wallet_transaction_record publish_version(
                 const string& account_to_publish_under,
                 const string& account_to_pay_with,
                 bool sign
                 );
         wallet_transaction_record collect_account_balances(
                 const string& account_name,
                 const function<bool( const balance_record& )> filter,
                 const string& memo_message,
                 bool sign
                 );
         wallet_transaction_record update_signing_key(
                 const string& authorizing_account_name,
                 const string& delegate_name,
                 const public_key_type& signing_key,
                 bool sign
                 );
         wallet_transaction_record asset_register(
                 const string& responsible_account_name,
                 const string& symbol,
                 const string& name,
                 const string& description,
                 const share_type max_supply,
                 const uint64_t precision,
                 bool user_issued,
                 bool sign
                 );
         wallet_transaction_record uia_issue_or_collect_fees(
                 const bool issue_new,
                 const asset& amount,
                 const string& generic_recipient,
                 const string& memo,
                 const bool sign
                 );
         wallet_transaction_record uia_issue_to_many(
                 const string& symbol,
                 const map<string, share_type>& addresses
                 );
         wallet_transaction_record uia_update_properties(
                 const string& paying_account,
                 const string& asset_symbol,
                 const asset_update_properties_operation& update_op,
                 const bool sign
                 );
         wallet_transaction_record uia_update_permission_or_flag(
                 const string& paying_account,
                 const string& asset_symbol,
                 const asset_record::flag_enum flag,
                 const bool add_instead_of_remove,
                 const bool update_authority_permission,
                 const bool sign
                 );
         wallet_transaction_record uia_update_whitelist(
                 const string& paying_account,
                 const string& asset_symbol,
                 const string& account_name,
                 const bool add_to_whitelist,
                 const bool sign
                 );
         wallet_transaction_record uia_retract_balance(
                 const balance_id_type& balance_id,
                 const string& account_name,
                 const bool sign
                 );
         /**
          *  ie: submit_bid( 10 BTC at 600.34 USD per BTC )
          *
          *  Requires the user have 6003.4 USD
          */
         wallet_transaction_record submit_bid(const string& from_account_name,
                 const string& real_quantity,
                 const string& quantity_symbol,
                 const string& price_per_unit,
                 const string& quote_symbol,
                 bool sign
                 );

         /**
          *  ie: submit_ask( 10 BTC at 600.34 USD per BTC )
          *
          *  Requires the user have 10 BTC + fees
          */
         wallet_transaction_record submit_ask(const string& from_account_name,
                 const string& real_quantity,
                 const string& quantity_symbol,
                 const string& price_per_unit,
                 const string& quote_symbol,
                 bool sign
                 );
         wallet_transaction_record submit_short(
                 const string& from_account_name,
                 const string& real_quantity_xts,
                 const string& collateral_symbol,
                 const string& apr,
                 const string& quote_symbol,
                 const string& price_limit,
                 bool sign
                 );
         wallet_transaction_record cover_short(
                 const string& from_account_name,
                 const string& real_quantity_usd,
                 const string& quote_symbol,
                 const order_id_type& short_id,
                 bool sign
                 );
         wallet_transaction_record add_collateral(
                 const string& from_account_name,
                 const order_id_type& short_id,
                 const string& real_quantity_collateral_to_add,
                 bool sign
                 );
         wallet_transaction_record cancel_market_orders(
                 const vector<order_id_type>& order_ids,
                 bool sign
                 );
         /**
          * @brief Perform arbitrarily many market operations in a single transaction
          * @param cancel_order_ids List of order IDs to cancel in the transaction
          * @param new_orders List of new orders to create. Each list element contains an order type and a list of
          * arguments. The arguments are the same as are taken by the wallet methods to execute that market operation in
          * its own transaction. If the final sign argument is provided in the arguments list, it will be ignored in
          * favor of the sign flag passed to batch_market_update.
          * @param sign Transaction will be signed and broadcast if true.
          * @return The resulting, potentially monstrously large, transaction
          */
         wallet_transaction_record batch_market_update(
                 const vector<order_id_type>& cancel_order_ids,
                 const vector<std::pair<order_type_enum,vector<string>>>& new_orders,
                 bool sign
                 );
         ///@} Transaction Generation Methods

         string                             get_key_label( const public_key_type& key )const;
         pretty_transaction                 to_pretty_trx( const wallet_transaction_record& trx_rec ) const;

         bool                               is_sending_address( const address& addr )const;
         bool                               is_receive_address( const address& addr )const;

         void                               scan_balances( const function<void( const balance_id_type&,
                                                                                const balance_record& )> callback )const;

         account_balance_record_summary_type get_spendable_account_balance_records( const string& account_name = "" )const;
         account_balance_summary_type       get_spendable_account_balances( const string& account_name = "" )const;

         account_balance_id_summary_type    get_account_balance_ids( const string& account_name = "" )const;

         account_vesting_balance_summary_type get_account_vesting_balances( const string& account_name = "" )const;

         account_balance_summary_type       get_account_yield( const string& account_name = "" )const;
         account_vote_summary_type          get_account_vote_summary( const string& account_name = "" )const;

         vector<escrow_summary>             get_escrow_balances( const string& account_name );

         map<order_id_type, market_order>   get_market_orders( const string& account_name, const string& quote_symbol,
                                                               const string& base_symbol, uint32_t limit )const;

         vector<wallet_transaction_record>  get_transaction_history( const string& account_name = string(),
                                                                     uint32_t start_block_num = 0,
                                                                     uint32_t end_block_num = -1,
                                                                     const string& asset_symbol = "" )const;
         vector<pretty_transaction>         get_pretty_transaction_history( const string& account_name = string(),
                                                                            uint32_t start_block_num = 0,
                                                                            uint32_t end_block_num = -1,
                                                                            const string& asset_symbol = "" )const;
         account_balance_summary_type       compute_historic_balance( const string &account_name,
                                                                      uint32_t block_num )const;

         void                               remove_transaction_record( const string& record_id );

         void                               repair_records( const optional<string>& collecting_account_name );
         uint32_t                           regenerate_keys( const string& account_name, uint32_t num_keys_to_regenerate );
         int32_t                            recover_accounts( int32_t number_of_accounts , int32_t max_number_of_attempts );

         wallet_transaction_record          recover_transaction( const string& transaction_id_prefix, const string& recipient_account );
         optional<variant_object>           verify_titan_deposit( const string& transaction_id_prefix );

         private_key_type get_private_key( const address& addr )const;
         public_key_type get_public_key( const address& addr) const;

         std::string login_start( const std::string& account_name );
         fc::variant login_finish( const public_key_type& server_key,
                                   const public_key_type& client_key,
                                   const fc::ecc::compact_signature& client_signature );

     private:
         unique_ptr<detail::wallet_impl> my;
   };

   typedef shared_ptr<wallet> wallet_ptr;
   typedef std::weak_ptr<wallet> wallet_weak_ptr;

} } // bts::wallet
