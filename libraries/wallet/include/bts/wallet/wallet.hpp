#pragma once
#include <bts/blockchain/chain_database.hpp>
#include <bts/wallet/wallet_db.hpp>
#include <bts/blockchain/config.hpp>


namespace bts { namespace wallet {
   using namespace bts::blockchain;

   namespace detail { class wallet_impl; }

   /** takes 4 parameters, current block, last block, current trx, last trx */
   typedef std::function<void(uint32_t,uint32_t,uint32_t,uint32_t)> scan_progress_callback;

   class wallet
   {
      public:
         wallet( const chain_database_ptr& chain );
         virtual ~wallet();

         void           set_data_directory( const fc::path& data_dir );
         void           get_data_directory()const;
         fc::path       get_wallet_filename_for_user( const std::string& username )const;

         std::string    get_name()const;
         void           open( const std::string& wallet_name, const std::string& password = "password" );
         void           create( const std::string& wallet_name, const std::string& key_password = "password" );
         void           open_file( const fc::path& file, const std::string& password  = "password");
         void           backup_wallet( const fc::path& file );

         bool           is_open()const;
         bool           close();

         bool           unlock( const std::string& password,
                                const fc::microseconds& tiemout = fc::seconds(30) );
         fc::time_point unlocked_until()const;

         void           lock();
         bool           is_unlocked()const;
         bool           is_locked()const;

         /** scans the records (not the transactions) looking for assets, accounts, and names
          * registered to any private key controlled by this wallet
          */
         void                       scan_state();
         void                       scan( const block_summary& summary );
         void                       scan_block( const full_block& );
         void                       scan_chain( uint32_t block_num,
                                                scan_progress_callback cb = scan_progress_callback() );
         uint32_t                   get_last_scanned_block_number()const;

         fc::time_point_sec         next_block_production_time()const;

         /** sign a block if this wallet controls the key for the active delegate, or throw */
         void                       sign_block( signed_block_header& header )const;

         wallet_contact_record      create_contact( const std::string& name,
                                                    const extended_public_key& contact_pub_key = extended_public_key() );

         void                       set_contact_extended_send_key( const std::string&,
                                                                   const extended_public_key& contact_pub_key );

         std::vector<std::string>   get_contacts()const;
         wallet_contact_record      get_contact( uint32_t contact_index );
         void                       add_sending_address( const address&, int32_t contact_index = 0 );
         void                       add_sending_address( const address&, const std::string& label );

         void                       import_bitcoin_wallet( const fc::path& dir, const std::string& passphrase );
         void                       import_wif_key( const std::string& wif, const std::string& contact_name );
         void                       import_private_key( const fc::ecc::private_key& priv_key, const std::string& contact_name );
         void                       import_private_key( const fc::ecc::private_key& priv_key, int32_t contact_index = 0 );

         signed_transaction         send_to_contact( const std::string& name, const asset& amount );

         signed_transaction         create_asset( const std::string& symbol, const std::string& name,
                                                  const std::string& description, const fc::variant& data,
                                                  const std::string& issuer_name,
                                                  share_type max_share_supply = BTS_BLOCKCHAIN_MAX_SHARES );

         signed_transaction         issue_asset( const std::string& symbol,
                                                 share_type amount,
                                                 const std::string& name );

         bool                       is_my_address( const address& a )const;
         address                    get_new_address( const std::string& name = "" );
         public_key_type            get_new_public_key( const std::string& name = "" );

         std::unordered_map<address,std::string>                            get_receive_addresses()const;
         std::unordered_map<address,std::string>                            get_send_addresses()const;

         std::unordered_map<transaction_id_type,wallet_transaction_record>  transactions()const;
         std::unordered_map<name_id_type,wallet_name_record>                names()const;
         std::unordered_map<account_id_type,wallet_account_record>          balances()const;
         std::unordered_map<asset_id_type,wallet_asset_record>              issued_assets()const;

         asset                                                              get_balance( asset_id_type asset_id = 0 );

         /** generate a transaction  */
         signed_transaction send_to_address( const asset& amnt, const address& public_key_addr, const std::string& memo = "" );

         /**
          * if the active_key is null then the active key will be made the same as the master key.
          * if the name already exists then it will be updated if this wallet controls the active key
          * or master key
          */
         signed_transaction reserve_name( const std::string& name, const fc::variant& json_data,
                                          bool as_delegate = false );

         signed_transaction update_name( const std::string& name,
                                         fc::optional<fc::variant> json_data,
                                         fc::optional<public_key_type> active = fc::optional<public_key_type>(),
                                         bool as_delegate = false );


         void               sign_transaction( signed_transaction& trx, const std::unordered_set<address>& req_sigs );

      private:
         std::unique_ptr<detail::wallet_impl> my;
   };

   typedef std::shared_ptr<wallet> wallet_ptr;

} } // bts::wallet
