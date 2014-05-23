#pragma once
#include <bts/blockchain/address.hpp>
#include <fc/optional.hpp>

namespace bts { namespace blockchain {

   struct market_index_key
   {
      market_index_key( const price& price_arg = price(), 
                        const address& owner_arg = address() )
      :order_price(price_arg),owner(owner_arg){}

      price   order_price;
      address owner;

      friend bool operator == ( const market_index_key& a, const market_index_key& b )
      {
         return a.order_price == b.order_price && a.owner == b.owner;
      }
      friend bool operator < ( const market_index_key& a, const market_index_key& b )
      {
         return a.order_price < b.order_price && a.owner == b.owner;
      }
   };

   struct order_record 
   {
      order_record():balance(0){}
      bool is_null() const { return 0 == balance; }

      share_type   balance;
      name_id_type delegate_id;
   };
   typedef fc::optional<order_record> oorder_record;

   struct collateral_record
   {
      collateral_record():collateral_balance(0),payoff_balance(0){}
      bool is_null() const { return 0 == payoff_balance; }

      share_type    collateral_balance;
      share_type    payoff_balance;
      name_id_type  delegate_id;
   };
   typedef fc::optional<collateral_record> ocollateral_record;

} } // bts::blockchain

FC_REFLECT( bts::blockchain::market_index_key, (order_price)(owner) )
FC_REFLECT( bts::blockchain::order_record, (balance)(delegate_id) )
FC_REFLECT( bts::blockchain::collateral_record, (collateral_balance)(payoff_balance)(delegate_id) );
