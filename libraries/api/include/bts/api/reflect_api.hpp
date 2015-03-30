#pragma once

#include <bts/api/common_api.hpp>
#include <map>
#include <set>
#include <fc/api.hpp>
#include <fc/optional.hpp>
#include <fc/variant.hpp>

namespace bts { namespace api {
   using namespace bts::blockchain;
   using fc::variant_object;

   class blockchain_api 
   {
      public:
      variant_object                                 about() const;
      variant_object                                 get_info() const;
      variant_object                                 validate_address(string address) const;
      variant_object                                 blockchain_get_info() const ;
      map<string, string>                            blockchain_list_feed_prices() const ;
      optional<account_record>                       blockchain_get_account(string account) const ;
      map<account_id_type, string>                   blockchain_get_slate(string slate) const ;
      balance_record                                 blockchain_get_balance(address balance_id) const ;
      unordered_map<balance_id_type, balance_record> blockchain_list_balances(string asset,uint32_t limit) const;
      void ntp_update_time();

      pair<vector<market_order>,vector<market_order>> blockchain_market_order_book(string quote_symbol, string base_symbol, uint32_t limit ) const;
      vector<market_order> blockchain_market_list_covers(string quote_symbol, string base_symbol, uint32_t limit ) const;
      vector<market_order> blockchain_market_list_bids(string quote_symbol, string base_symbol, uint32_t limit ) const;
      vector<market_order> blockchain_market_list_asks(string quote_symbol, string base_symbol, uint32_t limit) const;
      vector<market_order> blockchain_market_list_shorts(string quote_symbol, uint32_t limit ) const;
   };

   class wallet_api 
   {
      public:
      void wallet_unlock(uint32_t timeout, const std::string& passphrase);
   };
} } // bts::api
FC_API( bts::api::blockchain_api, 
        (about)
        (get_info)
        (validate_address)
        (blockchain_get_info)
        (blockchain_list_feed_prices)
        (blockchain_get_account)
        (blockchain_get_slate)
        (blockchain_market_order_book)
        (blockchain_market_list_covers)
        (blockchain_market_list_bids)
        (blockchain_market_list_asks)
        (blockchain_market_list_shorts)
      )
FC_API( bts::api::wallet_api, (wallet_unlock) )

namespace bts { namespace api {
   using std::string;
   using fc::optional;
   using std::map;
   using std::set;

   struct permissions
   {
       string      password;
       set<string> access;
   };

   struct login_api 
   {
      public:
         login_api( map<string,permissions> perms,
                    common_api* capi = nullptr ):_permissions(perms),_capi(capi){};

         set<string> login( const string& user, const string& password )
         {
           auto itr = _permissions.find( user );
           FC_ASSERT( itr != _permissions.end() );
           FC_ASSERT( itr->second.password == password );
           if( itr->second.access.end() != itr->second.access.find( "blockchain" ) )
              _blockchain = _capi;
           if( itr->second.access.end() != itr->second.access.find( "wallet" ) )
              _wallet = _capi;
           return itr->second.access;
         }

         fc::api<bts::api::blockchain_api> get_blockchain()const
         {
            FC_ASSERT( _blockchain, "no permission to access blockchain" );
            return *_blockchain;
         }
         fc::api<bts::api::wallet_api>     get_wallet()const
         {
            FC_ASSERT( _wallet, "no permission to access wallet" );
            return *_wallet;
         }

         optional<fc::api<bts::api::blockchain_api>> _blockchain;
         optional<fc::api<bts::api::wallet_api>>     _wallet;
         map<string,permissions>                     _permissions;
         common_api* _capi = nullptr;
   };
} } // bts::api

FC_REFLECT( bts::api::permissions, (password)(access) );
FC_API( bts::api::login_api, (login)(get_blockchain)(get_wallet) )
