#pragma once
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/account_record.hpp>
#include <bts/blockchain/balance_record.hpp>
#include <bts/blockchain/block_record.hpp>
#include <bts/blockchain/asset_record.hpp>
#include <bts/rpc/rpc_client.hpp>
#include <bts/wallet/config.hpp>

#define BTS_LIGHT_WALLET_PORT 8899
#define BTS_LIGHT_WALLET_DEFAULT_FEE  50000 // 0.5 XTS

namespace bts { namespace light_wallet {
   using namespace bts::blockchain;

   struct light_wallet_data
   {
       vector<char>                                 encrypted_private_key;
       account_record                               user_account;
       fc::time_point                               last_balance_sync_time;
       fc::time_point                               last_transaction_sync_time;
       map<balance_id_type,balance_record>          balance_record_cache;
       map<transaction_id_type,transaction_record>  transaction_record_cache;
       map<asset_id_type,asset_record>              asset_record_cache;
       map<string,pair<price,fc::time_point> >      price_cache;
       map<balance_id_type,memo_status>             memos;
   };

   struct light_transaction_summary
   {
       time_point_sec when;
       string         from;
       string         to;
       double         amount;
       string         symbol;
       double         fee;
       string         fee_symbol;
       string         memo;
       string         status; // pending, confirmed, error
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

         void create( const fc::path& wallet_json, 
                      const string& password, 
                      const string& brain_seed, const string& salt = string() );

         void request_register_account( const string& account_name );
         
         void transfer( double amount, 
                        const string& symbol, 
                        const string& to_account_name, 
                        const string& memo );

         void sync_balance( bool resync_all = false);
         void sync_transactions();

         oprice get_median_feed_price( const string& symbol );
         asset  get_fee( const string& symbol );

         set<string,double> balance()const;

         optional<asset_record> get_asset_record( const string& symbol );

         bts::rpc::rpc_client             _rpc;
         fc::path                         _wallet_file;
         optional<fc::ecc::private_key>   _private_key;
         optional<light_wallet_data>      _data;
   };

} }
FC_REFLECT( bts::light_wallet::light_wallet_data,
            (encrypted_private_key)
            (user_account)
            (balance_record_cache)
            (transaction_record_cache)
            (asset_record_cache)
            (price_cache)
            (memos) );

FC_REFLECT( bts::light_wallet::light_transaction_summary, (when)(from)(to)(amount)(symbol)(fee)(fee_symbol)(memo)(status) );
