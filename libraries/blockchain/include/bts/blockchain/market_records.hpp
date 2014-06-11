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
         if( a.order_price < b.order_price ) return true;
         if( a.order_price > b.order_price ) return false;
         return  a.owner < b.owner;
      }
   };


   struct order_record 
   {
      order_record():balance(0){}
      order_record( share_type b, name_id_type d )
      :balance(b),delegate_id(d){}

      bool is_null() const { return 0 == balance; }

      share_type       balance;
      account_id_type delegate_id;
   };
   typedef fc::optional<order_record> oorder_record;

   enum order_type_enum
   {
      bid_order,
      ask_order,
      short_order,
      cover_order
   };

   struct market_order 
   {
      order_type_enum  type;
      market_index_key market_index;
      order_record     state;
   };

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

FC_REFLECT_ENUM( bts::blockchain::order_type_enum, (bid_order)(ask_order)(short_order)(cover_order) );
FC_REFLECT( bts::blockchain::market_index_key, (order_price)(owner) )
FC_REFLECT( bts::blockchain::order_record, (balance)(delegate_id) )
FC_REFLECT( bts::blockchain::collateral_record, (collateral_balance)(payoff_balance)(delegate_id) );
FC_REFLECT( bts::blockchain::market_order, (type)(market_index)(state) );
