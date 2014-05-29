#pragma once

#include <bts/blockchain/chain_database.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/wallet/pretty.hpp>
#include <bts/wallet/wallet_db.hpp>

namespace bts { namespace wallet {
   using namespace bts::blockchain;

   namespace detail { class wallet_impl; }

   class wallet
   {
      public:
         wallet( chain_database_ptr chain );
         virtual ~wallet();

         /**
          *  Wallet File Management
          */
         ///@{
         void           set_data_directory( const fc::path& data_dir );
         fc::path       get_data_directory()const;

         void           create( const std::string& wallet_name, 
                                const std::string& password,
                                const std::string& brainkey = std::string() );

         void           create_file( const fc::path& wallet_file_name,
                                const std::string& password,
                                const std::string& brainkey = std::string() );

         void           open( const std::string& wallet_name );
         void           open_file( const fc::path& wallet_filename );

         void           close();

         void           export_to_json( const fc::path& export_file_name ) const;

         void           create_from_json( const fc::path& path, 
                                          const std::string& name );

         bool           is_open()const;
         std::string    get_wallet_name()const;
         fc::path       get_wallet_filename()const;
         ///@}
         
         /**
          *  @name Utility Methods
          */
         ///@{ 
         account_id_type select_delegate_vote()const;

         bool is_valid_symbol( const std::string& asset_symbol )const;
         bool is_receive_account( const std::string& account_name )const;

         /**
          *  A valid account is any named account registered in the blockchain or
          *  any local named account.
          */
         bool is_valid_account( const std::string& account_name )const;

         /**
          * Account names are limited the same way as domain names.
          */
         bool is_valid_account_name( const std::string& account_name )const;

         fc::ecc::private_key get_account_private_key( const std::string& account_name )const;
         public_key_type      get_account_public_key( const std::string& account_name )const;
         
         /**
          *  @returns the priority fee paid denominated in the given asset symbol.
          */
         asset                get_priority_fee( const std::string& symbol )const;
         ///@}

         /**
          *  Lock management & security
          */
         ///@{
         void           unlock( const std::string& password,
                                const fc::microseconds& timeout = fc::microseconds::maximum() );
         void           lock();
         void           change_passphrase(const std::string& new_passphrase);

         bool           is_unlocked()const;
         bool           is_locked()const;
         fc::time_point unlocked_until()const;
         ///@}

         void      scan_state();
         void      scan_chain( uint32_t start = 0, uint32_t end = -1 );
         uint32_t  get_last_scanned_block_number()const;

         ///@{ account management
         public_key_type  create_account( const std::string& account_name );
         void             import_account( const std::string& account_name, 
                                          const std::string& wif_private_key );


         address          get_new_address( const std::string& account_name );

         /**
          *  A contact is an account for which we do not have the private key.
          */
         void             add_contact_account( const std::string& account_name, 
                                               const public_key_type& key );

         void             rename_account( const std::string& old_contact_name, 
                                           const std::string& new_contact_name );
         ///@}  
         
         /** 
          * Return general information about the wallet 
          **/
         fc::variant get_info()const;


         /**
          *  Block Generation API
          */
         ///@{

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

         std::map<std::string,public_key_type> list_receive_accounts()const;
         std::map<std::string,public_key_type> list_contact_accounts()const;

         void import_bitcoin_wallet( const fc::path& wallet_dat,
                                     const std::string& wallet_dat_passphrase,
                                     const std::string& account_name );

         public_key_type import_private_key( const private_key_type& key, 
                                             const std::string& account_name );

         public_key_type import_wif_private_key( const std::string& wif_key, 
                                                 const std::string& account_name );

         ///@}

         /**
          *  Transaction Generation Methods
          */
         ///@{
         std::vector<signed_transaction> transfer( share_type amount_to_transfer,
                                                   const std::string& amount_to_transfer_symbol,
                                                   const std::string& from_account_name,
                                                   const std::string& to_account_name,
                                                   const std::string& memo_message,
                                                   bool sign );

         signed_transaction       create_asset( const std::string& symbol,
                                                const std::string& asset_name,
                                                const std::string& description,
                                                const fc::variant& data,
                                                const std::string& issuer_name,
                                                share_type max_share_supply = BTS_BLOCKCHAIN_MAX_SHARES,
                                                const bool sign = true );

         signed_transaction       issue_asset( share_type amount, 
                                               const std::string& symbol,                                               
                                               const std::string& to_account_name,
                                               const bool sign = true );

         owallet_account_record    get_account( const std::string& account_name );

         /**
          * if the active_key is null then the active key will be made the same as the master key.
          * if the name already exists then it will be updated if this wallet controls the active key
          * or master key
          */
         signed_transaction register_account( const std::string& account_name,
                                              const fc::variant& json_data,
                                              bool as_delegate = false,
                                              const bool sign = true );

         signed_transaction update_registered_account( const std::string& account_name,
                                                       fc::optional<fc::variant> json_data,
                                                       fc::optional<public_key_type> active = fc::optional<public_key_type>(),
                                                       bool as_delegate = false,
                                                       const bool sign = true );

         signed_transaction create_proposal( const std::string& delegate_account_name,
                                             const std::string& subject,
                                             const std::string& body,
                                             const std::string& proposal_type,
                                             const fc::variant& data,
                                             const bool sign = true );

         signed_transaction vote_proposal( const std::string& delegate_account_name, 
                                           proposal_id_type proposal_id, 
                                           uint8_t vote,
                                           const bool sign = true);


         ///@} Transaction Generation Methods
 
         pretty_transaction                      to_pretty_trx( wallet_transaction_record trx_rec,
                                                                int number = 0 );



         // std::string  get_symbol( asset_id_type asset_id )const;

         /*
         void                                         set_delegate_trust_status(const std::string& delegate_name, fc::optional<int32_t> trust_level);
         delegate_trust_status                        get_delegate_trust_status(const std::string& delegate_name) const;
         std::map<std::string, delegate_trust_status> list_delegate_trust_status() const;
         */

         bool                                       is_sending_address( const address& addr )const;
         bool                                       is_receive_address( const address& addr )const;


         /**
          *  Bitcoin compatiblity
          */
         ///@{
         //address                                    get_new_address( const std::string& account_name );
         //public_key_type                            get_new_public_key( const std::string& account_name );

         /*
         std::unordered_map<address,std::string>    get_receive_addresses()const;
         std::unordered_map<address,std::string>    get_send_addresses()const;
         */

         asset                                      get_balance( const std::string& symbol = BTS_ADDRESS_PREFIX,
                                                                 const std::string& account_name  = std::string() )const;

         std::vector<asset>                         get_all_balances( const std::string& account_name = std::string() )const;
         ///@}

         std::vector<wallet_transaction_record>     get_transaction_history()const;

         /*
         fc::optional<address>                      get_owning_address( const balance_id_type& id )const;
         fc::optional<wallet_account_record>        get_account_record( const address& addr)const;

         std::unordered_map<transaction_id_type,wallet_transaction_record>  transactions( const std::string& account_name = std::string() )const;
         */

         /*
         std::unordered_map<name_id_type,       wallet_name_record>         names( const std::string& account_name = "*" )const;
         std::unordered_map<asset_id_type,      wallet_asset_record>        assets( const std::string& account_name = "*" )const;
         */

         /** signs transaction with the specified keys for the specified addresses */
         void  sign_transaction( signed_transaction& trx, const std::unordered_set<address>& req_sigs );
         fc::ecc::private_key get_private_key( const address& addr )const;

      private:
         std::unique_ptr<detail::wallet_impl> my;
   };

   typedef std::shared_ptr<wallet> wallet_ptr;

} } // bts::wallet

//FC_REFLECT( bts::wallet::delegate_trust_status, (user_trust_level) )
