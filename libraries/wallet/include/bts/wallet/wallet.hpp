#pragma once

#include <bts/blockchain/chain_database.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/wallet/pretty.hpp>
#include <bts/wallet/wallet_db.hpp>
#include <fc/signals.hpp>

namespace bts { namespace wallet {
   using namespace bts::blockchain;

   namespace detail { class wallet_impl; }

   /** args: current_block, last_block */
   typedef function<void(uint32_t,uint32_t)> scan_progress_callback;

   typedef map<string, int64_t> account_vote_summary_type;
   typedef map<string, map<string, share_type>> account_balance_summary_type;

   enum delegate_status_flags
   {
       any_delegate_status = 0x00,
       enabled_delegate_status = 0x01,
       active_delegate_status = 0x02,
       disabled_delegate_status = 0x04,
       inactive_delegate_status = 0x08
   };

   class wallet
   {
      public:
         wallet( chain_database_ptr chain );
         virtual ~wallet();

         //Emitted when wallet is locked or unlocked. Argument is true if wallet is now locked; false otherwise.
         fc::signal<void( bool )>  wallet_lock_state_changed;

         /**
          *  To generate predictable test results we need an option
          *  to use deterministic keys rather than purely random
          *  one-time keys.
          */
         void use_deterministic_one_time_keys( bool state );

         /**
          *  Wallet File Management
          */
         ///@{
         void    set_data_directory( const path& data_dir );
         path    get_data_directory()const;

         void    create( const string& wallet_name, 
                         const string& password,
                         const string& brainkey = string() );

         void    create_file( const path& wallet_file_name,
                              const string& password,
                              const string& brainkey = string() );

         void    open( const string& wallet_name );
         void    open_file( const path& wallet_filename );

         void    close();

         bool    is_open()const;
         string  get_wallet_name()const;
         path    get_wallet_filename()const;

         void    export_to_json( const path& filename )const;
         void    create_from_json( const path& filename, const string& wallet_name, const string& passphrase );
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

         void set_setting(const string& name, const variant& value);
         fc::optional<variant> get_setting(const string& name);

         /**
          *  @name Utility Methods
          */
         ///@{ 
         delegate_slate select_delegate_vote()const;

         bool is_receive_account( const string& account_name )const;
         bool is_valid_account( const string& account_name )const;
         bool is_unique_account( const string& account_name )const;

         /**
          * Account names are limited the same way as domain names.
          */
         bool is_valid_account_name( const string& account_name )const;

         private_key_type get_account_private_key( const string& account_name )const;
         public_key_type  get_account_public_key( const string& account_name )const;

         public_key_summary get_public_key_summary( const public_key_type& pubkey ) const;
         vector<public_key_type> get_public_keys_in_account( const string& account_name )const;
         
         void    set_priority_fee( const asset& fee );
         asset   get_priority_fee()const;
         ///@}

         owallet_transaction_record lookup_transaction( const transaction_id_type& trx_id )const;

         vector<wallet_transaction_record>          get_pending_transactions()const;
         void                                       clear_pending_transactions();
         map<transaction_id_type, fc::exception>    get_pending_transaction_errors()const;

         void      scan_state( const time_point_sec& received_time );
         void      scan_chain( uint32_t start = 0, uint32_t end = -1,
                               const scan_progress_callback& progress_callback = scan_progress_callback() );
         uint32_t  get_last_scanned_block_number()const;

         void      scan_transaction( uint32_t block_num, const transaction_id_type& transaction_id );
         void      scan_transactions( uint32_t block_num, const string& transaction_id_prefix );

         ///@{ account management
         public_key_type  create_account( const string& account_name, 
                                          const variant& private_data = variant() );

         void account_set_favorite ( const string& account_name,
                                     const bool is_favorite );

         address          get_new_address( const string& account_name );
         public_key_type  get_new_public_key( const string& account_name );

         /**
          *  A contact is an account for which we do not have the private key.
          */
         void     add_contact_account( const string& account_name, 
                                       const public_key_type& key,
                                       const variant& private_data = variant() );

         void     remove_contact_account( const string& account_name );

         void     rename_account( const string& old_contact_name, 
                                  const string& new_contact_name );
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
         void set_delegate_transaction_scanning( bool enabled = false );

         ///@param delegates_to_retrieve Type is delegate_status_flags. Uses int type to allow ORing multiple flags
         vector<wallet_account_record> get_my_delegates( int delegates_to_retrieve = any_delegate_status )const;
         ///@param delegates_to_retrieve Type is delegate_status_flags. Uses int type to allow ORing multiple flags
         vector<private_key_type> get_my_delegate_private_keys( int delegates_to_retrieve = any_delegate_status )const;

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

         void import_bitcoin_wallet( const path& wallet_dat,
                                     const string& wallet_dat_passphrase,
                                     const string& account_name );
         void import_multibit_wallet( const path& wallet_dat,
                                     const string& wallet_dat_passphrase,
                                     const string& account_name );
         void import_electrum_wallet( const path& wallet_dat,
                                     const string& wallet_dat_passphrase,
                                     const string& account_name );
         void import_armory_wallet( const path& wallet_dat,
                                     const string& wallet_dat_passphrase,
                                     const string& account_name );
       
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
         signed_transaction  transfer_asset_to_address( double real_amount_to_transfer,
                                          const string& amount_to_transfer_symbol,
                                          const string& from_account_name,
                                          const address& to_address,
                                          const string& memo_message,
                                          bool sign );
         /**
          * This transfer works like a bitcoin sendmany transaction combining multiple inputs
          * and producing a single output.
          */
         signed_transaction  transfer_asset_to_many_address( const string& amount_to_transfer_symbol,
                                                     const string& from_account_name,
                                                     const unordered_map< address, double >& to_address_amounts,
                                                     const string& memo_message,
                                                     bool sign );
         /**
          *  This transfer works like a bitcoin transaction combining multiple inputs
          *  and producing a single output.
          */
         signed_transaction  transfer_asset( double real_amount_to_transfer,
                                              const string& amount_to_transfer_symbol,
                                              const string& from_account_name,
                                              const string& to_account_name,
                                              const string& memo_message,
                                              bool sign );

         signed_transaction  withdraw_delegate_pay( const string& delegate_name,
                                                    double amount_to_withdraw,
                                                    const string& withdraw_to_account_name,
                                                    const string& memo_message,
                                                    bool sign );

         signed_transaction  create_asset( const string& symbol,
                                           const string& asset_name,
                                           const string& description,
                                           const variant& data,
                                           const string& issuer_name,
                                           double max_share_supply,
                                           int64_t  precision,
                                           bool is_market_issued = false,
                                           bool sign = true );

         signed_transaction  issue_asset( double amount, 
                                          const string& symbol,                                               
                                          const string& to_account_name,
                                          const string& memo_message,
                                          bool sign = true );

         /**
          *  ie: submit_bid( 10 BTC at 600.34 USD per BTC )
          *
          *  Requires the user have 6003.4 USD
          */
         signed_transaction  submit_bid( const string& from_account_name,
                                         double real_quantity, 
                                         const string& quantity_symbol,
                                         double price_per_unit,
                                         const string& quote_symbol,
                                         bool sign = true );

         /**
          *  ie: submit_ask( 10 BTC at 600.34 USD per BTC )
          *
          *  Requires the user have 10 BTC + fees
          */
         signed_transaction  submit_ask( const string& from_account_name,
                                         double real_quantity, 
                                         const string& quantity_symbol,
                                         double price_per_unit,
                                         const string& quote_symbol,
                                         bool sign = true );

         /**
          *  ie: submit_short( 10 USD at 600.34 USD per XTS )
          *
          *  Requires the user have 10 / 600.34 XTS + fees
          */
         signed_transaction  submit_short( const string& from_account_name,
                                         double real_quantity_usd, 
                                         double price_per_unit,
                                         const string& quote_symbol,
                                         bool sign = true );

         signed_transaction  cancel_market_order( const address& owner_address );

         owallet_account_record get_account( const string& account_name )const;

         /**
          * if the active_key is null then the active key will be made the same as the master key.
          * if the name already exists then it will be updated if this wallet controls the active key
          * or master key
          */
         wallet_transaction_record register_account( const string& account_name,
                                              const variant& json_data,
                                              uint8_t delegate_pay_rate,
                                              const string& pay_with_account_name,
                                              bool sign = true );

         void update_account_private_data( const string& account_to_update,
                                           const variant& private_data );

         wallet_transaction_record update_registered_account( const string& account_name,
                                      const string& pay_from_account,
                                      optional<variant> public_data,
                                      uint8_t delegate_pay_rate = 255,
                                      optional<public_key_type> active = optional<public_key_type>(),
                                      bool sign = true );

         signed_transaction create_proposal( const string& delegate_account_name,
                                             const string& subject,
                                             const string& body,
                                             const string& proposal_type,
                                             const variant& data,
                                             bool sign = true );

         signed_transaction vote_proposal( const string& delegate_account_name, 
                                           proposal_id_type proposal_id, 
                                           proposal_vote::vote_type vote,
                                           const string& message = string(),
                                           bool sign = true);


         ///@} Transaction Generation Methods
         string              get_key_label( const public_key_type& key )const;
         pretty_transaction to_pretty_trx( const wallet_transaction_record& trx_rec ) const;


         void      set_delegate_approval( const string& delegate_name, int approved );
         int      get_delegate_approval( const string& delegate_name )const;

         bool      is_sending_address( const address& addr )const;
         bool      is_receive_address( const address& addr )const;

         /**
          *  Bitcoin compatibility
          */
         ///@{
         //address                                    get_new_address( const string& account_name );
         //public_key_type                            get_new_public_key( const string& account_name );

         /*
         unordered_map<address,string>    get_receive_addresses()const;
         unordered_map<address,string>    get_send_addresses()const;
         */
         
         account_balance_summary_type       get_account_balances( const string& account_name = "" )const;

         account_vote_summary_type          get_account_vote_summary( const string& account_name = "" )const;

         vector<market_order_status>        get_market_orders( const string& quote, const string& base )const;

         vector<wallet_transaction_record>  get_transaction_history( const string& account_name = string(),
                                                                        uint32_t start_block_num = 0,
                                                                        uint32_t end_block_num = -1 )const;
         vector<pretty_transaction>         get_pretty_transaction_history( const string& account_name = string(),
                                                                               uint32_t start_block_num = 0,
                                                                               uint32_t end_block_num = -1 )const;

         optional<wallet_account_record>    get_account_record( const address& addr)const;
         /*
         optional<address>                  get_owning_address( const balance_id_type& id )const;

         unordered_map<transaction_id_type,wallet_transaction_record>  transactions( const string& account_name = string() )const;
         */

         /*
         unordered_map<account_id_type,     wallet_name_record>         names( const string& account_name = "*" )const;
         unordered_map<asset_id_type,       wallet_asset_record>        assets( const string& account_name = "*" )const;
         */

         /** signs transaction with the specified keys for the specified addresses */
         void             sign_transaction( signed_transaction& trx, const unordered_set<address>& req_sigs );
         private_key_type get_private_key( const address& addr )const;

      private:
         unique_ptr<detail::wallet_impl> my;
   };

   typedef shared_ptr<wallet> wallet_ptr;

} } // bts::wallet
