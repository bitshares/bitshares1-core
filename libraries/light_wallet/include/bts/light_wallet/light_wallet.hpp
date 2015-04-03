#pragma once
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/account_record.hpp>
#include <bts/blockchain/balance_record.hpp>
#include <bts/blockchain/block_record.hpp>
#include <bts/blockchain/asset_record.hpp>
#include <bts/blockchain/chain_database.hpp>
#include <bts/rpc/rpc_client.hpp>
#include <bts/wallet/config.hpp>
#include <bts/wallet/wallet_records.hpp>

#define BTS_LIGHT_WALLET_PORT 8899
#define BTS_LIGHT_WALLET_STORAGE_VERSION 1

namespace bts { namespace light_wallet {
   using namespace bts::blockchain;

   struct account_data
   {
       vector<char>                                                           encrypted_private_key;
       account_record                                                         user_account;
       map<transaction_id_type, fc::variant_object>                           transaction_record_cache;
       std::multimap<asset_id_type,transaction_id_type>                       transaction_index;
       unordered_map<transaction_id_type,wallet::transaction_ledger_entry>    scan_cache;
   };
   struct light_wallet_data
   {
       fc::time_point_sec                                last_balance_sync_time;
       uint32_t                                          last_transaction_sync_block = 0;
       unordered_map<string,pair<price,fc::time_point>>  price_cache;
       unordered_map<string,account_data>                accounts;
   };

   class light_wallet
   {
      public:
         light_wallet(std::function<void(string,string)> persist_function,
                      std::function<string(string)> restore_function,
                      std::function<bool(string)> can_restore_function);
         ~light_wallet();

         void connect( const string& host, const string& user = "any", const string& pass = "none", uint16_t port = 0,
                       const public_key_type& server_key = public_key_type() );
         bool is_connected()const;
         void set_disconnect_callback(std::function<void (fc::exception_ptr)> callback);
         void disconnect();

         void open();
         void save();
         void close();
         bool is_open()const;

         void unlock( const string& password );
         void lock();
         bool is_unlocked()const;
         void change_password( const string& new_password );

         void create(const std::string& account_name,
                     const string& password,
                     const string& brain_seed);

         bool request_register_account(const std::string& account_name);
         account_record& account(const string& account_name);
         account_record& fetch_account(const string& account_name);
         vector<const account_record*> account_records() const;

         fc::variant_object prepare_transfer( const string& amount,
                                              const string& symbol,
                                              const string& from_account_name,
                                              const string& to_account_name,
                                              const string& memo );
         bool complete_transfer(const std::string& account_name,
                                const string& password,
                                const fc::variant_object& transaction_bundle);

         bool sync_balance( bool resync_all = false);
         bool sync_transactions(bool resync_all = false);

         oprice get_median_feed_price( const string& symbol );
         asset  get_fee( const string& symbol );

         map<string, pair<double, double> > balance(const std::string& account_name)const;
         bts::wallet::transaction_ledger_entry summarize(const std::string& account_name, const fc::variant_object& transaction_bundle);
         vector<wallet::transaction_ledger_entry> transactions(const std::string& account_name, const string& symbol );

         optional<asset_record> get_asset_record( const string& symbol ) const;
         optional<asset_record> get_asset_record( const asset_id_type& id ) const;
         vector<string>         all_asset_symbols() const;

         oaccount_record get_account_record(const string& identifier );

         bts::rpc::rpc_client             _rpc;
         optional<fc::sha512>             _wallet_key;
         optional<light_wallet_data>      _data;
         mutable pending_chain_state_ptr  _chain_cache;
         oaccount_record                  _relay_fee_collector;
         asset                            _relay_fee;
         asset                            _network_fee;

         std::function<void(string, string)>       persist;
         std::function<bool(string)>               can_restore;
         std::function<string(string)>             restore;

         void fetch_welcome_package();
         fc::ecc::private_key derive_private_key(const string& prefix_string, int sequence_number);
   private:
         fc::ecc::private_key create_one_time_key(const std::string& account_name, const std::string& key_id);
         asset get_network_fee( const string& symbol );
         fc::variants batch_active_addresses(const char* call_name, fc::variant last_sync, vector<std::string>& account_records);
         fc::ecc::private_key active_key(const string& account_name);

         map<string, account_record> _account_cache;
         bool _is_connected = false;
   };

} }
FC_REFLECT( bts::light_wallet::account_data,
            (encrypted_private_key)
            (user_account)
            (transaction_record_cache)
            (transaction_index)
            (scan_cache) );
FC_REFLECT( bts::light_wallet::light_wallet_data,
            (last_balance_sync_time)
            (last_transaction_sync_block)
            (price_cache)
            (accounts) );
