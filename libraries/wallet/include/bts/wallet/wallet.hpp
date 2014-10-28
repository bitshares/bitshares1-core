#pragma once

#include <bts/blockchain/chain_database.hpp>
#include <bts/mail/message.hpp>
#include <bts/wallet/pretty.hpp>
#include <bts/wallet/transaction_builder.hpp>

#include <fc/signals.hpp>

namespace bts { namespace wallet {

   using namespace bts::blockchain;

   namespace detail { class wallet_impl; }

   typedef map<string, vector<balance_record>> account_balance_record_summary_type;
   typedef map<string, vector<balance_id_type>> account_balance_id_summary_type;
   typedef map<string, map<asset_id_type, share_type>> account_balance_summary_type;
   typedef map<string, int64_t> account_vote_summary_type;
   typedef std::pair<order_type_enum, vector<string>> order_description;

   enum delegate_status_flags
   {
       any_delegate_status      = 0x00,
       enabled_delegate_status  = 0x01,
       active_delegate_status   = 0x02,
       disabled_delegate_status = 0x04,
       inactive_delegate_status = 0x08
   };

   class wallet
   {
      public:
         wallet( chain_database_ptr chain, bool enabled = true );
         virtual ~wallet();

         //Emitted when wallet is locked or unlocked. Argument is true if wallet is now locked; false otherwise.
         fc::signal<void( bool )>  wallet_lock_state_changed;
         //Emitted when wallet claims a new transaction. Argument is new ledger entry.
         fc::signal<void( ledger_entry )> wallet_claimed_transaction;
         //Emitted when someone (partially or fully) fills your short, thereby giving you a margin position
         fc::signal<void( ledger_entry )> update_margin_position;

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

         void    auto_backup( const string& reason )const;
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
         asset                  get_transaction_fee( const asset_id_type& desired_fee_asset_id = 0 )const;
         bool                   asset_can_pay_fee( const asset_id_type& desired_fee_asset_id = 0 )const;

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
         private_key_type get_active_private_key( const string& account_name )const;
         public_key_type  get_account_public_key( const string& account_name )const;

         public_key_summary get_public_key_summary( const public_key_type& pubkey ) const;
         vector<public_key_type> get_public_keys_in_account( const string& account_name )const;
         ///@}

         wallet_transaction_record get_transaction( const string& transaction_id_prefix )const;

         vector<wallet_transaction_record>          get_pending_transactions()const;
         map<transaction_id_type, fc::exception>    get_pending_transaction_errors()const;

         void scan_chain( uint32_t start = 0, uint32_t end = -1, bool fast_scan = false );

         wallet_transaction_record         scan_transaction( const string& transaction_id_prefix, bool overwrite_existing );
         transaction_ledger_entry          scan_transaction_experimental( const string& transaction_id_prefix, bool overwrite_existing );

         void add_transaction_note_experimental( const string& transaction_id_prefix, const string& note );
         set<pretty_transaction_experimental> transaction_history_experimental( const string& account_name );
         pretty_transaction_experimental to_pretty_transaction_experimental( const transaction_ledger_entry& record );

         vector<wallet_transaction_record> get_transactions( const string& transaction_id_prefix );

         ///@{ account management
         public_key_type  create_account( const string& account_name,
                                          const variant& private_data = variant() );

         void update_account_private_data( const string& account_to_update,
                                           const variant& private_data );

         void account_set_favorite ( const string& account_name,
                                     const bool is_favorite );

         wallet_account_record get_account( const string& account_name )const;

         /**
          *  A contact is an account for which we do not have the private key.
          */
         void     add_contact_account( const string& account_name,
                                       const public_key_type& key,
                                       const variant& private_data = variant() );

         void     remove_contact_account( const string& account_name );

         void     rename_account( const string& old_contact_name,
                                  const string& new_contact_name );

         owallet_account_record  get_account_for_address( address addr )const;
         ///@}

         /**
          * Return general information about the wallet
          **/
         variant get_info()const;

         /**
          *  Block Generation API
          */
         ///@{
         void set_delegate_block_production( const string& delegate_id, bool enabled = true );

         ///@param delegates_to_retrieve Type is delegate_status_flags. Uses int type to allow ORing multiple flags
         vector<wallet_account_record> get_my_delegates( int delegates_to_retrieve = any_delegate_status )const;

         optional<time_point_sec> get_next_producible_block_timestamp( const vector<wallet_account_record>& delegate_records )const;

         /** sign a block if this wallet controls the key for the active delegate, or throw */
         void sign_block( signed_block_header& header )const;
         ///@}

         /**
          *  Account management API
          */
         ///@{
         vector<string> list() const; // list wallets

         vector<wallet_account_record> list_accounts()const;
         vector<wallet_account_record> list_favorite_accounts()const;
         vector<wallet_account_record> list_unregistered_accounts()const;
         vector<wallet_account_record> list_my_accounts()const;

         std::shared_ptr<transaction_builder> create_transaction_builder()
         {
            return std::make_shared<transaction_builder>(my.get());
         }

         uint32_t import_bitcoin_wallet(
                 const path& wallet_dat,
                 const string& wallet_dat_passphrase,
                 const string& account_name
                 );
         uint32_t import_multibit_wallet(
                 const path& wallet_dat,
                 const string& wallet_dat_passphrase,
                 const string& account_name
                 );
         uint32_t import_electrum_wallet(
                 const path& wallet_dat,
                 const string& wallet_dat_passphrase,
                 const string& account_name
                 );
         uint32_t import_armory_wallet(
                 const path& wallet_dat,
                 const string& wallet_dat_passphrase,
                 const string& account_name
                 );

         void import_keyhotee( const string& firstname,
                            const string& middlename,
                            const string& lastname,
                            const string& brainkey,
                            const string& keyhoteeid );

         public_key_type import_private_key( const private_key_type& key,
                                             const string& account_name,
                                             bool create_account = false );

         public_key_type import_wif_private_key( const string& wif_key,
                                                 const string& account_name,
                                                 bool create_account = false );
         ///@}

         /**
          *  Transaction Generation Methods
          */
         ///@{
         /**
          *  Multi-Part transfers provide additional security by not combining inputs, but they
          *  show up to the user as multiple unique transfers.  This is an advanced feature
          *  that should probably have some user interface support to merge these transfers
          *  into one logical transfer.
          */
         vector<signed_transaction> multipart_transfer( double real_amount_to_transfer,
                                                         const string& amount_to_transfer_symbol,
                                                         const string& from_account_name,
                                                         const string& to_account_name,
                                                         const string& memo_message,
                                                         bool sign );

         /**
          *  This transfer works like a bitcoin transaction combining multiple inputs
          *  and producing a single output. The only different aspect with transfer_asset is that
          *  this will send to a address.
          */
         wallet_transaction_record transfer_asset_to_address(
                 double real_amount_to_transfer,
                 const string& amount_to_transfer_symbol,
                 const string& from_account_name,
                 const address& to_address,
                 const string& memo_message,
                 vote_selection_method selection_method,
                 bool sign = true
                 );
         /**
          * This transfer works like a bitcoin sendmany transaction combining multiple inputs
          * and producing a single output.
          */
         wallet_transaction_record transfer_asset_to_many_address(
                 const string& amount_to_transfer_symbol,
                 const string& from_account_name,
                 const unordered_map<address, double>& to_address_amounts,
                 const string& memo_message,
                 bool sign = true
                 );

         /**
          *  This transfer works like a bitcoin transaction combining multiple inputs
          *  and producing a single output.
          */
         wallet_transaction_record burn_asset(
                 double real_amount_to_transfer,
                 const string& amount_to_transfer_symbol,
                 const string& paying_account_name,
                 const string& for_or_against,
                 const string& to_account_name,
                 const string& public_message,
                 bool anonymous = true,
                 bool sign = true
                 );

         /**
          * if the active_key is null then the active key will be made the same as the master key.
          * if the name already exists then it will be updated if this wallet controls the active key
          * or master key
          */
         wallet_transaction_record register_account(
                 const string& account_name,
                 const variant& json_data,
                 share_type delegate_pay_rate,
                 const string& pay_with_account_name,
                 bts::blockchain::account_type new_account_type = titan_account,
                 bool sign = true
                 );

         wallet_transaction_record update_registered_account(
                 const string& account_name,
                 const string& pay_from_account,
                 optional<variant> public_data,
                 share_type delegate_pay_rate,
                 bool sign = true
                 );

         wallet_transaction_record update_active_key(
                 const std::string& account_to_update,
                 const std::string& pay_from_account,
                 const std::string& new_active_key,
                 bool sign = true
                 );

         wallet_transaction_record withdraw_delegate_pay(
                 const string& delegate_name,
                 double amount_to_withdraw,
                 const string& withdraw_to_account_name,
                 bool sign = true
                 );

         wallet_transaction_record publish_feeds(
                 const string& account,
                 map<string,double> amount_per_xts,
                 bool sign = true
                 );
         wallet_transaction_record publish_price(
                 const string& account,
                 double amount_per_xts,
                 const string& amount_asset_symbol,
                 bool sign = true
                 );
         wallet_transaction_record publish_slate(
                 const string& account_to_publish_under,
                 const string& account_to_pay_with,
                 bool sign = true
                 );
         wallet_transaction_record publish_version(
                 const string& account_to_publish_under,
                 const string& account_to_pay_with,
                 bool sign = true
                 );
         wallet_transaction_record create_asset(
                 const string& symbol,
                 const string& asset_name,
                 const string& description,
                 const variant& data,
                 const string& issuer_name,
                 double max_share_supply,
                 int64_t precision,
                 bool is_market_issued = false,
                 bool sign = true
                 );
         wallet_transaction_record issue_asset(
                 double amount,
                 const string& symbol,
                 const string& to_account_name,
                 const string& memo_message,
                 bool sign = true
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
                 bool sign = true
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
                 bool sign = true
                 );
         wallet_transaction_record submit_short(const string& from_account_name,
                                                const string& real_quantity_xts,
                                                const string& collateral_symbol,
                                                const string& apr,
                                                const string& quote_symbol,
                                                const string& price_limit = 0,
                                                bool sign = true
                                                );
         wallet_transaction_record cover_short(
                 const string& from_account_name,
                 const string& real_quantity_usd,
                 const string& quote_symbol,
                 const order_id_type& short_id,
                 bool sign = true
                 );
         wallet_transaction_record add_collateral(
                 const string& from_account_name,
                 const order_id_type& short_id,
                 const string& real_quantity_collateral_to_add,
                 bool sign = true
                 );
         wallet_transaction_record cancel_market_orders(
                 const vector<order_id_type>& order_ids,
                 bool sign = true
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
                 bool sign = true
                 );
#if 0
         wallet_transaction_record create_proposal(
                 const string& delegate_account_name,
                 const string& subject,
                 const string& body,
                 const string& proposal_type,
                 const variant& data,
                 bool sign = true
                 );
         wallet_transaction_record vote_proposal(
                 const string& delegate_account_name,
                 proposal_id_type proposal_id,
                 proposal_vote::vote_type vote,
                 const string& message = string(),
                 bool sign = tru
                 );
#endif
         ///@} Transaction Generation Methods

         string                             get_key_label( const public_key_type& key )const;
         pretty_transaction                 to_pretty_trx( const wallet_transaction_record& trx_rec ) const;

         void                               set_account_approval( const string& account_name, int8_t approval );
         int8_t                             get_account_approval( const string& account_name )const;

         bool                               is_sending_address( const address& addr )const;
         bool                               is_receive_address( const address& addr )const;

         account_balance_record_summary_type get_account_balance_records( const string& account_name = "", bool include_empty = true )const;
         account_balance_id_summary_type    get_account_balance_ids( const string& account_name = "", bool include_empty = true )const;
         account_balance_summary_type       get_account_balances( const string& account_name = "", bool include_empty = true )const;

         account_balance_summary_type       get_account_yield( const string& account_name = "" )const;
         asset                              asset_worth( const asset& base, const string& price_in_symbol )const;
         asset                              get_account_net_worth( const string& account_name, const string& symbol )const;
         account_vote_summary_type          get_account_vote_summary( const string& account_name = "" )const;

         map<order_id_type, market_order>   get_market_orders( const string& account_name, uint32_t limit)const;
         map<order_id_type, market_order>   get_market_orders( const string& quote, const string& base,
                                                               uint32_t limit, const string& account_name )const;

         vector<wallet_transaction_record>  get_transaction_history( const string& account_name = string(),
                                                                     uint32_t start_block_num = 0,
                                                                     uint32_t end_block_num = -1,
                                                                     const string& asset_symbol = "" )const;
         vector<pretty_transaction>         get_pretty_transaction_history( const string& account_name = string(),
                                                                            uint32_t start_block_num = 0,
                                                                            uint32_t end_block_num = -1,
                                                                            const string& asset_symbol = "" )const;

         void                               remove_transaction_record( const string& record_id );

         uint32_t                           regenerate_keys( const string& account_name, uint32_t max_number_of_attempts );
         int32_t                            recover_accounts(int32_t number_of_accounts , int32_t max_number_of_attempts);

         wallet_transaction_record          recover_transaction( const string& transaction_id_prefix, const string& recipient_account );
         optional<variant_object>           verify_titan_deposit( const string& transaction_id_prefix );

         vote_summary get_vote_proportion( const string& account_name );

         private_key_type get_private_key( const address& addr )const;

         std::string login_start( const std::string& account_name );
         fc::variant login_finish( const public_key_type& server_key,
                                   const public_key_type& client_key,
                                   const fc::ecc::compact_signature& client_signature );

         mail::message mail_create( const string& sender,
                                    const string& subject,
                                    const string& body,
                                    const mail::message_id_type& reply_to = mail::message_id_type());
         mail::message mail_encrypt( const public_key_type& recipient, const mail::message& plaintext );
         mail::message mail_open( const address& recipient, const mail::message& ciphertext );
         mail::message mail_decrypt( const address& recipient, const mail::message& ciphertext );

     private:
         unique_ptr<detail::wallet_impl> my;
   };

   typedef shared_ptr<wallet> wallet_ptr;
   typedef std::weak_ptr<wallet> wallet_weak_ptr;

} } // bts::wallet

FC_REFLECT_ENUM( bts::wallet::vote_selection_method, (vote_none)(vote_all)(vote_random)(vote_recommended) )
