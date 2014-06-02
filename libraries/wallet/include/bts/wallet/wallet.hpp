#pragma once

#include <bts/blockchain/chain_database.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/wallet/pretty.hpp>
#include <bts/wallet/wallet_db.hpp>

namespace bts { namespace wallet {
   using namespace bts::blockchain;

   namespace detail { class wallet_impl; }

    

   /** args: current_block, last_block */
   typedef function<void(uint32_t,uint32_t)> scan_progress_callback;

   class wallet
   {
      public:
         wallet( chain_database_ptr chain );
         virtual ~wallet();

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

         void    export_to_json( const path& export_file_name ) const;

         void    create_from_json( const path& path, 
                                   const string& name );

         bool    is_open()const;
         string  get_wallet_name()const;
         path    get_wallet_filename()const;
         ///@}
         
         /**
          *  @name Utility Methods
          */
         ///@{ 
         account_id_type select_delegate_vote()const;

         bool is_receive_account( const string& account_name )const;
         bool is_valid_account( const string& account_name )const;
         bool is_unique_account( const string& account_name )const;

         /**
          * Account names are limited the same way as domain names.
          */
         bool is_valid_account_name( const string& account_name )const;

         private_key_type get_account_private_key( const string& account_name )const;
         public_key_type  get_account_public_key( const string& account_name )const;
         
         /**
          *  @returns the priority fee paid denominated in the given asset symbol.
          */
         asset   get_priority_fee( const string& symbol = BTS_ADDRESS_PREFIX )const;
         ///@}

         /**
          *  Lock management & security
          */
         ///@{
         void     unlock( const string& password,
                          microseconds timeout = microseconds::maximum() );
         void     lock();
         void     change_passphrase(const string& new_passphrase);

         bool     is_unlocked()const;
         bool     is_locked()const;

         fc::time_point unlocked_until()const;
         ///@}

         void      scan_state();
         void      scan_chain( uint32_t start = 0, uint32_t end = -1,
                              scan_progress_callback cb = scan_progress_callback() );
         uint32_t  get_last_scanned_block_number()const;

         ///@{ account management
         public_key_type  create_account( const string& account_name, 
                                          const variant& private_data = variant() );
         void             import_account( const string& account_name, 
                                          const string& wif_private_key );


         address  get_new_address( const string& account_name );

         /**
          *  A contact is an account for which we do not have the private key.
          */
         void     add_contact_account( const string& account_name, 
                                       const public_key_type& key );

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

         /**
          *  If this wallet has any delegate keys, this method will return the time
          *  at which this wallet may produce a block.
          */
         time_point_sec next_block_production_time()const;

         /** sign a block if this wallet controls the key for the active delegate, or throw */
         void               sign_block( signed_block_header& header )const;

         ///@} Block Generation API


         /**
          *  Account management API
          */
         ///@{

         vector<string> list() const; // list wallets

         vector<wallet_account_record> list_receive_accounts()const;
         vector<wallet_account_record> list_contact_accounts()const;

         void import_bitcoin_wallet( const path& wallet_dat,
                                     const string& wallet_dat_passphrase,
                                     const string& account_name );

         public_key_type import_private_key( const private_key_type& key, 
                                             const string& account_name );

         public_key_type import_wif_private_key( const string& wif_key, 
                                                 const string& account_name );

         ///@}

         /**
          *  Transaction Generation Methods
          */
         ///@{
         vector<signed_transaction> transfer( share_type amount_to_transfer,
                                              const string& amount_to_transfer_symbol,
                                              const string& from_account_name,
                                              const string& to_account_name,
                                              const string& memo_message,
                                              bool sign );

         signed_transaction       create_asset( const string& symbol,
                                                const string& asset_name,
                                                const string& description,
                                                const variant& data,
                                                const string& issuer_name,
                                                share_type max_share_supply = BTS_BLOCKCHAIN_MAX_SHARES,
                                                const bool sign = true );

         signed_transaction       issue_asset( share_type amount, 
                                               const string& symbol,                                               
                                               const string& to_account_name,
                                               const bool sign = true );

         owallet_account_record    get_account( const string& account_name );

         /**
          * if the active_key is null then the active key will be made the same as the master key.
          * if the name already exists then it will be updated if this wallet controls the active key
          * or master key
          */
         wallet_transaction_record register_account( const string& account_name,
                                              const variant& json_data,
                                              bool  as_delegate, 
                                              const string& pay_with_account_name,
                                              const bool sign = true );

         wallet_transaction_record update_registered_account( const string& account_name,
                                                       const string& pay_from_account,
                                                       optional<variant> json_data,
                                                       optional<public_key_type> active = optional<public_key_type>(),
                                                       bool as_delegate = false,
                                                       const bool sign = true );

         signed_transaction create_proposal( const string& delegate_account_name,
                                             const string& subject,
                                             const string& body,
                                             const string& proposal_type,
                                             const variant& data,
                                             const bool sign = true );

         signed_transaction vote_proposal( const string& delegate_account_name, 
                                           proposal_id_type proposal_id, 
                                           uint8_t vote,
                                           const bool sign = true);


         ///@} Transaction Generation Methods
 
         pretty_transaction                      to_pretty_trx( wallet_transaction_record trx_rec ) const;


         void      set_delegate_trust_level(const string& delegate_name, 
                                            int32_t trust_level);

         int32_t   get_delegate_trust_level(const string& delegate_name) const;

         bool      is_sending_address( const address& addr )const;
         bool      is_receive_address( const address& addr )const;


         /**
          *  Bitcoin compatiblity
          */
         ///@{
         //address                                    get_new_address( const string& account_name );
         //public_key_type                            get_new_public_key( const string& account_name );

         /*
         std::unordered_map<address,string>    get_receive_addresses()const;
         std::unordered_map<address,string>    get_send_addresses()const;
         */

         vector<asset>                         get_balance( const string& symbol = string("*"),
                                                            const string& account_name  = string("*") )const;
         ///@}

         vector<wallet_transaction_record>     get_transaction_history( const string& account_name = string() )const;
         vector<pretty_transaction>     get_pretty_transaction_history( const string& account_name = string() )const;

         /*
         optional<address>                      get_owning_address( const balance_id_type& id )const;
         optional<wallet_account_record>        get_account_record( const address& addr)const;

         std::unordered_map<transaction_id_type,wallet_transaction_record>  transactions( const string& account_name = string() )const;
         */

         /*
         std::unordered_map<account_id_type,       wallet_name_record>         names( const string& account_name = "*" )const;
         std::unordered_map<asset_id_type,      wallet_asset_record>        assets( const string& account_name = "*" )const;
         */

         /** signs transaction with the specified keys for the specified addresses */
         void             sign_transaction( signed_transaction& trx, const std::unordered_set<address>& req_sigs );
         private_key_type get_private_key( const address& addr )const;

      private:
         std::unique_ptr<detail::wallet_impl> my;
   };

   typedef std::shared_ptr<wallet> wallet_ptr;

} } // bts::wallet

//FC_REFLECT( bts::wallet::delegate_trust_status, (user_trust_level) )
