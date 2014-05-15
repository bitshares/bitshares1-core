#pragma once
#include <bts/blockchain/chain_database.hpp>
#include <bts/wallet/wallet_db.hpp>
#include <bts/blockchain/config.hpp>


namespace bts { namespace wallet {
   using namespace bts::blockchain;

   namespace detail { class wallet_impl; }

   /** takes 4 parameters, current block, last block, current trx, last trx */
   typedef std::function<void(uint32_t,uint32_t,uint32_t,uint32_t)> scan_progress_callback;

   /**
    * When transferring a balance from one individual to another it must be
    * divided up into many smaller transactions to maximize privacy.  A group
    * of smaller transactions is considered an invoice.
    *
    * These transactions should be broadcast over time rather than all at once
    * in order to maximize user privacy.
    */
   struct invoice_summary
   {
      std::map<transaction_id_type, signed_transaction> payments;
      std::string                                       from_account;
      std::string                                       to_account;
      invoice_index_type                                sending_invoice_index;
      payment_index_type                                last_sending_payment_index;
   };

   class wallet
   {
      public:
         wallet( const chain_database_ptr& chain );
         virtual ~wallet();

         /**
          *  Wallet File Management
          */
         ///@{
         void           set_data_directory( const fc::path& data_dir );
         fc::path       get_data_directory()const;

         void           open( const std::string& wallet_name, const std::string& password = "password" );
         void           create( const std::string& wallet_name, const std::string& key_password = "password" );
         void           open_file( const fc::path& file, const std::string& password  = "password");
         void           backup_wallet( const fc::path& file );

         fc::path       get_filename()const;
         std::string    get_name()const;

         bool           is_open()const;
         bool           close();
         ///@}

         /**
          *  Lock management & security
          */
         ///@{
         void           unlock( const std::string& password,
                                const fc::microseconds& tiemout = fc::seconds(30) );
         fc::time_point unlocked_until()const;

         void           lock();
         bool           is_unlocked()const;
         bool           is_locked()const;

         void change_password( const std::string& new_password );
         ///@}

         /**
          *  Blockchain Scanning API
          *
          *  Looks throught the blockchain for assets, names, and other information
          *  that is relevant to this wallet.
          */
         ///@{
         /** scans the records (not the transactions) looking for assets, accounts, and names
          * registered to any private key controlled by this wallet, this is faster than
          * scanning for transactions.
          */
         void               scan_state();
         void               scan_balances();
         void               scan_names();
         void               scan_assets();

         void               scan_block( const full_block& );
         void               scan_chain( uint32_t block_num,
                                        scan_progress_callback cb = scan_progress_callback() );
         ///@}

         /**
          *  Block Generation API
          */
         ///@{
         uint32_t           get_last_scanned_block_number()const;

         /**
          *  If this wallet has any delegate keys, this method will return the time
          *  at which this wallet may produce a block.
          */
         fc::time_point_sec next_block_production_time()const;

         /** sign a block if this wallet controls the key for the active delegate, or throw */
         void               sign_block( signed_block_header& header )const;

         ///@} Block Generation API


         /**
          *  Account management API
          */
         ///@{
         wallet_account_record    create_receive_account( const std::string& account_name );
         void                     create_sending_account( const std::string& account_name, const extended_public_key& );

         std::map<std::string,extended_address> list_receive_accounts( uint32_t start = 0, uint32_t count = -1 )const;
         std::map<std::string,extended_address> list_sending_accounts( uint32_t start = 0, uint32_t count = -1 )const;
         wallet_account_record    get_account( const std::string& account_name )const;
         


         void import_bitcoin_wallet( const fc::path& wallet_dat, 
                                     const std::string& wallet_dat_passphrase,
                                     const std::string& account_name = "*", 
                                     const std::string& invoice_memo = "" );

         void import_private_key( const private_key_type& key,
                                  const std::string& account_name = "*", 
                                  const std::string& invoice_memo = "" );

         void import_wif_private_key( const std::string& wif_key,
                                  const std::string& account_name = "*", 
                                  const std::string& invoice_memo = "" );
         ///@}

         /**
          *  Transaction Generation Methods
          */
         ///@{
         enum wallet_flag
         {
            sign_and_broadcast    = 0,
            do_not_broadcast      = 1,
            do_not_sign           = 2
         };

         invoice_summary          transfer( const std::string& to_account_name, 
                                            const asset& amount, 
                                            const std::string& invoice_memo = "",
                                            const std::string& from_account_name = "*",
                                            wallet_flag options = sign_and_broadcast );

         signed_transaction       create_asset( const std::string& symbol, 
                                                const std::string& asset_name,
                                                const std::string& description, 
                                                const fc::variant& data,
                                                const std::string& issuer_name,
                                                share_type max_share_supply = BTS_BLOCKCHAIN_MAX_SHARES,
                                                const std::string& account_name = "*",
                                                wallet_flag options = sign_and_broadcast );

         signed_transaction       issue_asset( const std::string& symbol, 
                                               share_type amount, 
                                               const std::string& account_name );
         /**
          * if the active_key is null then the active key will be made the same as the master key.
          * if the name already exists then it will be updated if this wallet controls the active key
          * or master key
          */
         signed_transaction reserve_name( const std::string& name, 
                                          const fc::variant& json_data, 
                                          bool as_delegate = false,
                                          const std::string& account_name = "*",
                                          wallet_flag flag = sign_and_broadcast );

         signed_transaction update_name( const std::string& name, 
                                         fc::optional<fc::variant> json_data, 
                                         fc::optional<public_key_type> active = fc::optional<public_key_type>(), 
                                         bool as_delegate = false, 
                                         wallet_flag flag = sign_and_broadcast );

         ///@} Transaction Generation Methods

         bool                                       is_sending_address( const address& a )const;
         bool                                       is_receive_address( const address& a )const;

         /**
          *  Bitcoin compatiblity
          */
         ///@{
         address                                    get_new_address( const std::string& account_name = "", uint32_t invoice = 0 );
         public_key_type                            get_new_public_key( const std::string& account_name = "", uint32_t invoice = 0 );

         std::unordered_map<address,std::string>    get_receive_addresses()const;
         std::unordered_map<address,std::string>    get_send_addresses()const;

         asset                                      get_balance( const std::string& account_name = "*", asset_id_type asset_id = 0 );
         ///@}

         std::unordered_map<transaction_id_type,wallet_transaction_record>  transactions( const std::string& account_name = "*" )const;
         std::unordered_map<name_id_type,       wallet_name_record>         names( const std::string& account_name = "*" )const;
         std::unordered_map<balance_id_type,    wallet_account_record>      balances( const std::string& account_name = "*" )const;
         std::unordered_map<asset_id_type,      wallet_asset_record>        issued_assets( const std::string& account_name = "*" )const;


         /** signs transaction with the specified keys for the specified addresses */
         void  sign_transaction( signed_transaction& trx, const std::unordered_set<address>& req_sigs );

      private:
         std::unique_ptr<detail::wallet_impl> my;
   };

   typedef std::shared_ptr<wallet> wallet_ptr;

} } // bts::wallet

FC_REFLECT_ENUM( bts::wallet::wallet::wallet_flag, (do_not_broadcast)(do_not_sign)(sign_and_broadcast) )
FC_REFLECT( bts::wallet::invoice_summary, (payments)(from_account)(to_account)(sending_invoice_index)(last_sending_payment_index) )
