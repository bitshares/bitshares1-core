#pragma once

#include <bts/wallet/wallet_db.hpp>

#include <bts/blockchain/account_operations.hpp>
#include <bts/blockchain/asset_operations.hpp>
#include <bts/blockchain/balance_operations.hpp>
#include <bts/blockchain/feed_operations.hpp>
#include <bts/blockchain/market_operations.hpp>

namespace bts { namespace wallet { namespace detail {

class wallet_impl : public chain_observer
{
   public:
      wallet*                                          self = nullptr;
      bool                                             _is_enabled = true;
      wallet_db                                        _wallet_db;
      chain_database_ptr                               _blockchain = nullptr;
      path                                             _data_directory;
      path                                             _current_wallet_path;
      fc::sha512                                       _wallet_password;
      fc::optional<fc::time_point>                     _scheduled_lock_time;
      fc::future<void>                                 _relocker_done;
      fc::future<void>                                 _scan_in_progress;

      unsigned                                         _num_scanner_threads = 1;
      vector<std::unique_ptr<fc::thread>>              _scanner_threads;
      float                                            _scan_progress = 1;

      bool                                             _dirty_balances = true;
      unordered_map<balance_id_type, balance_record>   _balance_records;

      bool                                             _dirty_accounts = true;
      vector<private_key_type>                         _stealth_private_keys;

      struct login_record
      {
          private_key_type key;
          fc::time_point_sec insertion_time;
      };
      std::map<public_key_type, login_record>          _login_map;
      fc::future<void>                                 _login_map_cleaner_done;
      const static short                               _login_cleaner_interval_seconds = 60;
      const static short                               _login_lifetime_seconds = 300;

      vector<function<void( void )>>                   _unlocked_upgrade_tasks;

      wallet_impl();

      void create_file( const path& wallet_file_name,
                        const string& password,
                        const string& brainkey = string() );

      void open_file( const path& wallet_filename );

      void reschedule_relocker();
      void relocker();

      virtual void block_pushed( const full_block& )override;
      virtual void block_popped( const pending_chain_state_ptr& )override;

      void scan_balances_experimental();

      void scan_market_transaction(
              const market_transaction& mtrx,
              uint32_t block_num,
              const time_point_sec block_time
              );

      secret_hash_type get_secret( uint32_t block_num,
                                   const private_key_type& delegate_key )const;

      void scan_block( uint32_t block_num );

      wallet_transaction_record scan_transaction(
              const signed_transaction& transaction,
              uint32_t block_num,
              const time_point_sec block_timestamp,
              bool overwrite_existing = false
              );

      void scan_block_experimental( uint32_t block_num,
                                    const map<private_key_type, string>& account_keys,
                                    const map<address, string>& account_balances,
                                    const set<string>& account_names );

      transaction_ledger_entry scan_transaction_experimental( const transaction_evaluation_state& eval_state,
                                                              uint32_t block_num,
                                                              const time_point_sec timestamp,
                                                              bool overwrite_existing );

      transaction_ledger_entry scan_transaction_experimental( const transaction_evaluation_state& eval_state,
                                                              uint32_t block_num,
                                                              const time_point_sec timestamp,
                                                              const map<private_key_type, string>& account_keys,
                                                              const map<address, string>& account_balances,
                                                              const set<string>& account_names,
                                                              bool overwrite_existing );

      void scan_transaction_experimental( const transaction_evaluation_state& eval_state,
                                          const map<private_key_type, string>& account_keys,
                                          const map<address, string>& account_balances,
                                          const set<string>& account_names,
                                          transaction_ledger_entry& record,
                                          bool store_record );

      bool scan_withdraw( const withdraw_operation& op, wallet_transaction_record& trx_rec, asset& total_fee, public_key_type& from_pub_key );
      bool scan_withdraw_pay( const withdraw_pay_operation& op, wallet_transaction_record& trx_rec, asset& total_fee );

      bool scan_deposit( const deposit_operation& op, wallet_transaction_record& trx_rec, asset& total_fee );

      bool scan_register_account( const register_account_operation& op, wallet_transaction_record& trx_rec );
      bool scan_update_account( const update_account_operation& op, wallet_transaction_record& trx_rec );

      bool scan_create_asset( const create_asset_operation& op, wallet_transaction_record& trx_rec );
      bool scan_issue_asset( const issue_asset_operation& op, wallet_transaction_record& trx_rec );

      bool scan_update_feed(const update_feed_operation& op, wallet_transaction_record& trx_rec );

      bool scan_bid( const bid_operation& op, wallet_transaction_record& trx_rec, asset& total_fee );
      bool scan_ask( const ask_operation& op, wallet_transaction_record& trx_rec, asset& total_fee );
      bool scan_short( const short_operation& op, wallet_transaction_record& trx_rec, asset& total_fee );
      bool scan_short_v1( const short_operation_v1& op, wallet_transaction_record& trx_rec, asset& total_fee );

      bool scan_burn( const burn_operation& op, wallet_transaction_record& trx_rec, asset& total_fee );

      vector<wallet_transaction_record> get_pending_transactions()const;

      void withdraw_to_transaction( const asset& amount_to_withdraw,
                                    const string& from_account_name,
                                    signed_transaction& trx,
                                    unordered_set<address>& required_signatures )const;

      wallet_contact_record generic_recipient_to_contact( const string& generic_recipient )const;

      public_key_type deposit_from_transaction( signed_transaction& transaction, const asset& amount,
                                                const wallet_account_record& sender, const wallet_contact_record& recipient,
                                                const string& memo );

      void authorize_update( unordered_set<address>& required_signatures, oaccount_record account, bool need_owner_key = false );

      void start_scan_task( const uint32_t start_block_num, const uint32_t limit );
      void scan_accounts();
      void scan_balances();

      void login_map_cleaner_task();

      void upgrade_version();
      void upgrade_version_unlocked();

      slate_id_type set_delegate_slate( signed_transaction& transaction, const vote_strategy strategy )const;
      slate_record get_delegate_slate( const vote_strategy strategy )const;

      private_key_type  get_new_private_key( const string& account_name );
      public_key_type   get_new_public_key( const string& account_name );
      address           get_new_address( const string& account_name, const string& label="" );

      void sign_transaction( signed_transaction& transaction, const unordered_set<address>& required_signatures )const;

      transaction_ledger_entry apply_transaction_experimental( const signed_transaction& transaction );

      void apply_order_to_builder(order_type_enum order_type,
                                  transaction_builder_ptr builder,
                                  const string& account_name,
                                  const string& balance,
                                  const string& order_price,
                                  const string& base_symbol,
                                  const string& quote_symbol,
                                  bool needs_satoshi_conversion,
                                  const string& short_price_limit = string()
                                 );

      // TODO:  Refactor and move somewhere else
      price str_to_relative_price( const string& str, const string& base_symbol, const string& quote_symbol );
};

} } } // bts::wallet::detail
