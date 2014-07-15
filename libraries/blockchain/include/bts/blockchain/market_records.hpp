#pragma once

#include <bts/blockchain/asset.hpp>
#include <bts/blockchain/types.hpp>

#include <fc/time.hpp>

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

   struct market_history_key
   {
       enum time_granularity_enum {
         each_block,
         each_hour,
         each_day
       };

       market_history_key( asset_id_type quote_id = 0,
                           asset_id_type base_id = 1,
                           time_granularity_enum granularity = each_block,
                           fc::time_point_sec timestamp = fc::time_point_sec())
         : quote_id(quote_id),
           base_id(base_id),
           granularity(granularity),
           timestamp(timestamp)
       {}

       asset_id_type quote_id;
       asset_id_type base_id;
       time_granularity_enum granularity;
       fc::time_point_sec timestamp;

       bool operator < ( const market_history_key& other ) const
       {
          if( base_id < other.base_id ) return true;
          if( base_id > other.base_id ) return false;
          if( quote_id < other.quote_id ) return true;
          if( quote_id > other.quote_id ) return false;
          if( granularity < other.granularity ) return true;
          if( granularity > other.granularity ) return false;
          return timestamp < other.timestamp;
       }
       bool operator == ( const market_history_key& other ) const
       {
         return quote_id == other.quote_id
             && base_id == other.base_id
             && granularity == other.granularity
             && timestamp == other.timestamp;
       }
   };

   struct market_history_record
   {
       market_history_record(price highest_bid = price(),
                             price lowest_ask = price(),
                             share_type volume = 0)
         : highest_bid(highest_bid),
           lowest_ask(lowest_ask),
           volume(volume)
       {}

       price highest_bid;
       price lowest_ask;
       share_type volume;

       bool operator == ( const market_history_record& other ) const
       {
         return highest_bid == other.highest_bid
              && lowest_ask == other.lowest_ask
                  && volume == other.volume;
       }
   };
   typedef fc::optional<market_history_record> omarket_history_record;
   typedef std::vector<std::pair<time_point_sec, market_history_record>> market_history_points;


   struct order_record 
   {
      order_record():balance(0){}
      order_record( share_type b )
      :balance(b){}

      bool is_null() const { return 0 == balance; }

      share_type       balance;
   };
   typedef fc::optional<order_record> oorder_record;

   enum order_type_enum
   {
      null_order,
      bid_order,
      ask_order,
      short_order,
      cover_order
   };

   struct market_order 
   {
      market_order( order_type_enum t, market_index_key k, order_record s )
      :type(t),market_index(k),state(s){}
      market_order( order_type_enum t, market_index_key k, order_record s, share_type c )
      :type(t),market_index(k),state(s),collateral(c){}
       
       
      market_order():type(null_order){}

      string            get_id()const;
      asset             get_balance()const; // funds available for this order
      price             get_price()const;
      price             get_highest_cover_price()const; // the price that consumes all collateral
      asset             get_quantity()const;
      asset             get_quote_quantity()const;
      address           get_owner()const { return market_index.owner; }

      order_type_enum       type;
      market_index_key      market_index;
      order_record          state;
      optional<share_type>  collateral; 
   };


   struct market_transaction
   {
      market_transaction(){}

      address  bid_owner;
      address  ask_owner;
      price    bid_price;
      price    ask_price;
      asset    bid_paid;
      asset    bid_received;
      asset    ask_paid;
      asset    ask_received;
      asset    fees_collected;
   };

   typedef optional<market_order> omarket_order;

   struct collateral_record
   {
      collateral_record(share_type c = 0, share_type p = 0):collateral_balance(c),payoff_balance(p){}
      bool is_null() const { return 0 == payoff_balance && 0 == collateral_balance; }

      share_type      collateral_balance;
      share_type      payoff_balance;
   };
   typedef fc::optional<collateral_record> ocollateral_record;

} } // bts::blockchain

FC_REFLECT_ENUM( bts::blockchain::order_type_enum, (null_order)(bid_order)(ask_order)(short_order)(cover_order) )
FC_REFLECT_ENUM( bts::blockchain::market_history_key::time_granularity_enum, (each_block)(each_hour)(each_day) )
FC_REFLECT( bts::blockchain::market_index_key, (order_price)(owner) )
FC_REFLECT( bts::blockchain::market_history_record, (highest_bid)(lowest_ask)(volume) )
FC_REFLECT( bts::blockchain::market_history_key, (quote_id)(base_id)(granularity)(timestamp) )
FC_REFLECT( bts::blockchain::order_record, (balance) )
FC_REFLECT( bts::blockchain::collateral_record, (collateral_balance)(payoff_balance) )
FC_REFLECT( bts::blockchain::market_order, (type)(market_index)(state)(collateral) )
FC_REFLECT_TYPENAME( std::vector<bts::blockchain::market_transaction> )
FC_REFLECT_TYPENAME( bts::blockchain::market_history_key::time_granularity_enum ) // http://en.wikipedia.org/wiki/Voodoo_programminqg
FC_REFLECT( bts::blockchain::market_transaction, 
            (bid_owner)
            (ask_owner)
            (bid_price)
            (ask_price)
            (bid_paid)
            (bid_received)
            (ask_paid)
            (ask_received)
            (fees_collected) 
          )
