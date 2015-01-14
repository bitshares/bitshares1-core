#pragma once
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/account_record.hpp>
#include <bts/blockchain/balance_record.hpp>
#include <bts/blockchain/block_record.hpp>
#include <bts/blockchain/asset_record.hpp>
#include <bts/rpc/rpc_client.hpp>
#include <bts/wallet/config.hpp>
#include <bts/wallet/wallet_records.hpp>

#define BTS_LIGHT_WALLET_PORT 8899
#define BTS_LIGHT_WALLET_DEFAULT_FEE  50000 // 0.5 XTS

namespace bts { namespace light_wallet {
   using namespace bts::blockchain;

   struct light_wallet_data
   {
       vector<char>                                 encrypted_private_key;
       account_record                               user_account;
       fc::time_point                               last_balance_sync_time;
       uint32_t                                     last_transaction_sync_block;
       map<balance_id_type,balance_record>          balance_record_cache;
       map<transaction_id_type, fc::variant_object> transaction_record_cache;
       map<asset_id_type,asset_record>              asset_record_cache;
       map<string,pair<price,fc::time_point> >      price_cache;
       map<balance_id_type,memo_status>             memos;

       std::map<pair<account_id_type,asset_id_type>,std::set<transaction_id_type>> transaction_index;
   };

   class light_wallet 
   {
      public:
         light_wallet();
         ~light_wallet();

         void connect( const string& host, const string& user = "any", const string& pass = "none", uint16_t port = 0 );
         bool is_connected()const;
         void disconnect();

         void open( const fc::path& wallet_json );
         void save();
         void close();
         bool is_open()const;

         void unlock( const string& password );
         void lock();
         bool is_unlocked()const;
         void change_password( const string& new_password );

         void create(const fc::path& wallet_json, const std::string& account_name,
                      const string& password,
                      const string& brain_seed, const string& salt = string() );

         bool request_register_account();
         account_record& account();
         account_record& fetch_account();
         
         void transfer( double amount, 
                        const string& symbol, 
                        const string& to_account_name, 
                        const string& memo );

         void sync_balance( bool resync_all = false);
         void sync_transactions();

         oprice get_median_feed_price( const string& symbol );
         asset  get_fee( const string& symbol );

         map<string,double> balance()const;
         vector<wallet::transaction_ledger_entry> transactions( const string& symbol );

         optional<asset_record> get_asset_record( const string& symbol );
         optional<asset_record> get_asset_record( const asset_id_type& id );

         oaccount_record get_account_record(const string& identifier );

         bts::rpc::rpc_client             _rpc;
         fc::path                         _wallet_file;
         optional<fc::ecc::private_key>   _private_key;
         optional<light_wallet_data>      _data;

   private:
         bts::wallet::transaction_ledger_entry summarize(const fc::variant_object& transaction_bundle);

         map<string, account_record> _account_cache;
   };

} }
FC_REFLECT( bts::light_wallet::light_wallet_data,
            (encrypted_private_key)
            (user_account)
            (last_balance_sync_time)
            (last_transaction_sync_block)
            (balance_record_cache)
            (transaction_record_cache)
            (asset_record_cache)
            (price_cache)
            (memos)
            (transaction_index) );
