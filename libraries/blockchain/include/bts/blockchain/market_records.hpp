#pragma once

#include <bts/blockchain/asset.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/types.hpp>

#include <fc/exception/exception.hpp>
#include <fc/io/enum_type.hpp>
#include <fc/time.hpp>

#include <tuple>

namespace bts { namespace blockchain {

   /** 
    *  Note: this data structure is used for operation serialization, modifications
    *        will break existing operations.
    */
   struct market_index_key
   {
      market_index_key( const price& price_arg = price(),
                        const address& owner_arg = address() )
      :order_price(price_arg),owner(owner_arg){}

      price                 order_price;
      address               owner;

      friend bool operator == ( const market_index_key& a, const market_index_key& b )
      {
        return std::tie(a.order_price, a.owner) == std::tie(b.order_price, b.owner);
      }
      friend bool operator < ( const market_index_key& a, const market_index_key& b )
      {
        return std::tie(a.order_price, a.owner) < std::tie(b.order_price, b.owner);
      }
   };

   struct market_index_key_ext : public market_index_key
   {
      market_index_key_ext( const market_index_key b = market_index_key(), 
                            optional<price> limit = optional<price>() )
      :market_index_key(b),limit_price(limit){}

      optional<price> limit_price;

      friend bool operator == ( const market_index_key_ext& a, const market_index_key_ext& b )
      {
          if( a.limit_price.valid() != b.limit_price.valid() ) return false;
          if( !a.limit_price.valid() ) return market_index_key( a ) == market_index_key( b );
          return std::tie( a.order_price, a.owner, *a.limit_price ) == std::tie( b.order_price, b.owner, *b.limit_price );
      }

      friend bool operator < ( const market_index_key_ext& a, const market_index_key_ext& b )
      {
          FC_ASSERT( a.limit_price.valid() == b.limit_price.valid() );
          if( !a.limit_price.valid() ) return market_index_key( a ) < market_index_key( b );
          return std::tie( a.order_price, a.owner, *a.limit_price ) < std::tie( b.order_price, b.owner, *b.limit_price );
      }
   };

   struct short_price_index_key
   {
      price             order_price;    // min(feed,limit)
      market_index_key  index;
      friend bool operator < ( const short_price_index_key& a, const short_price_index_key& b )
      {
        return std::tie(a.order_price, a.index ) < std::tie( b.order_price, b.index );
      }
      friend bool operator == ( const short_price_index_key& a, const short_price_index_key& b )
      {
        return std::tie(a.order_price, a.index ) == std::tie( b.order_price, b.index );
      }
   };

   struct expiration_index
   {
      asset_id_type      quote_id;
      time_point         expiration;
      market_index_key   key;

      friend bool operator < ( const expiration_index& a, const expiration_index& b )
      {
         return std::tie( a.quote_id, a.expiration, a.key )  < std::tie( b.quote_id, b.expiration, b.key );
      }
      friend bool operator == ( const expiration_index& a, const expiration_index& b )
      {
         return std::tie( a.quote_id, a.expiration, a.key )  == std::tie( b.quote_id, b.expiration, b.key );
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
         return std::tie(base_id, quote_id, granularity, timestamp) < std::tie(other.base_id, other.quote_id, other.granularity, other.timestamp);
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
                             price opening_price = price(),
                             price closing_price = price(),
                             share_type volume = 0)
         : highest_bid(highest_bid),
           lowest_ask(lowest_ask),
           opening_price(opening_price),
           closing_price(closing_price),
           volume(volume)
       {}

       price highest_bid;
       price lowest_ask;
       price opening_price;
       price closing_price;
       share_type volume;

       bool operator == ( const market_history_record& other ) const
       {
         return highest_bid == other.highest_bid
              && lowest_ask == other.lowest_ask
                  && volume == other.volume;
       }
   };
   typedef fc::optional<market_history_record> omarket_history_record;

   struct market_history_point
   {
       fc::time_point_sec timestamp;
       double highest_bid;
       double lowest_ask;
       double opening_price;
       double closing_price;
       share_type volume;
   };
   typedef vector<market_history_point> market_history_points;

   struct order_record
   {
      order_record():balance(0){}
      order_record( share_type b )
      :balance(b){}

      bool is_null() const { return balance == 0; }

      share_type            balance;
      optional<price>       limit_price;
      fc::time_point_sec    last_update;
   };
   typedef fc::optional<order_record> oorder_record;

   enum order_type_enum
   {
      null_order,
      bid_order,
      ask_order,
      short_order,
      cover_order,
      relative_bid_order,
      relative_ask_order
   };

   struct market_order
   {
      // bids, asks, rbids, rasks
      market_order( order_type_enum t, market_index_key k, order_record s )
      :type(t),market_index(k),state(s){}

      // shorts
      market_order( order_type_enum t, market_index_key k, order_record s, share_type c, price interest )
      :type(t),market_index(k),state(s),collateral(c),interest_rate(interest){}

      // covers
      market_order( order_type_enum t, market_index_key k, order_record s, share_type c, price interest, time_point_sec exp )
      :type(t),market_index(k),state(s),collateral(c),interest_rate(interest),expiration(exp){}

      market_order():type(null_order){}

      order_id_type get_id()const;
      string        get_small_id()const;
      asset         get_balance()const; // funds available for this order
      price         get_price( const price& base = price() )const;
      price         get_highest_cover_price()const; // the price that consumes all collateral
      asset         get_quantity( const price& base = price() )const;
      asset         get_quote_quantity( const price& base = price() )const;
      address       get_owner()const { return market_index.owner; }
      optional<price>  get_limit_price()const { return state.limit_price; }

      fc::enum_type<uint8_t, order_type_enum>   type = null_order;
      market_index_key                          market_index;
      order_record                              state;
      optional<share_type>                      collateral;
      optional<price>                           interest_rate;
      optional<time_point_sec>                  expiration;
   };

   struct market_transaction
   {
      address                                   bid_owner;
      address                                   ask_owner;
      price                                     bid_price;
      price                                     ask_price;
      asset                                     bid_paid;
      asset                                     bid_received;
      /** if bid_type == short, then collateral will be paid from short to cover positon */
      optional<asset>                           short_collateral;
      asset                                     ask_paid;
      asset                                     ask_received;
      /** any leftover collateral returned to owner after a cover */
      optional<asset>                           returned_collateral;
      fc::enum_type<uint8_t, order_type_enum>   bid_type = null_order;
      fc::enum_type<uint8_t, order_type_enum>   ask_type = null_order;
      asset                                     fees_collected;
   };
   typedef optional<market_order> omarket_order;

   struct order_history_record : public market_transaction
   {
      order_history_record(const market_transaction& market_trans = market_transaction(), fc::time_point_sec timestamp = fc::time_point_sec())
        : market_transaction(market_trans),
          timestamp(timestamp)
      {}

      fc::time_point_sec                        timestamp;
   };

   struct collateral_record
   {
      collateral_record(share_type c = 0,
                        share_type p = 0,
                        const price& apr = price(),
                        time_point_sec exp = time_point_sec())
          :collateral_balance(c),payoff_balance(p),interest_rate(apr),expiration(exp){}
      bool is_null() const { return 0 == payoff_balance && 0 == collateral_balance; }

      share_type      collateral_balance;
      share_type      payoff_balance;
      price           interest_rate;
      time_point_sec  expiration; // after expiration the collateral is forced to be called.
   };
   typedef fc::optional<collateral_record> ocollateral_record;

   struct market_status
   {
       market_status(){} // Null case
       market_status( asset_id_type quote, asset_id_type base )
       :quote_id(quote),base_id(base)
       {
           FC_ASSERT( quote > base );
       }

       bool is_null()const { return quote_id == base_id; }
       void update_feed_price( const optional<price>& feed )
       {
           current_feed_price = feed;
           if( current_feed_price.valid() )
               last_valid_feed_price = current_feed_price;
       }

       asset_id_type            quote_id;
       asset_id_type            base_id;
       optional<price>          current_feed_price;
       optional<price>          last_valid_feed_price;
       optional<fc::exception>  last_error;

       /**************************************************************************************/
       /* All of these are no longer used but need to be kept around for applying old blocks */
       share_type               ask_depth = 0;
       share_type               bid_depth = 0;
       price                    center_price;
       price minimum_ask()const
       {
           auto avg = center_price;
           avg.ratio *= 9;
           avg.ratio /= 10;
           return avg;
       }
       price maximum_bid()const
       {
           auto avg = center_price;
           avg.ratio *= 10;
           avg.ratio /= 9;
           return avg;
       }
       /**************************************************************************************/
   };
   typedef optional<market_status> omarket_status;

   struct api_market_status : public market_status {
       api_market_status(const market_status& market_stat = market_status())
         : market_status(market_stat)
       {}
       double                   current_feed_price;
       double                   last_valid_feed_price;
   };

} } // bts::blockchain

FC_REFLECT_ENUM( bts::blockchain::order_type_enum,
                 (null_order)
                 (bid_order)
                 (ask_order)
                 (short_order)
                 (cover_order)
                 (relative_bid_order)
                 (relative_ask_order)
               )

FC_REFLECT_ENUM( bts::blockchain::market_history_key::time_granularity_enum, (each_block)(each_hour)(each_day) )
FC_REFLECT( bts::blockchain::market_status, (quote_id)(base_id)(current_feed_price)(last_valid_feed_price)(last_error)(ask_depth)(bid_depth)(center_price) )
FC_REFLECT_DERIVED( bts::blockchain::api_market_status, (bts::blockchain::market_status), (current_feed_price)(last_valid_feed_price) )
FC_REFLECT( bts::blockchain::market_index_key, (order_price)(owner) )
FC_REFLECT_DERIVED( bts::blockchain::market_index_key_ext, (bts::blockchain::market_index_key), (limit_price) )
FC_REFLECT( bts::blockchain::market_history_record, (highest_bid)(lowest_ask)(opening_price)(closing_price)(volume) )
FC_REFLECT( bts::blockchain::market_history_key, (quote_id)(base_id)(granularity)(timestamp) )
FC_REFLECT( bts::blockchain::market_history_point, (timestamp)(highest_bid)(lowest_ask)(opening_price)(closing_price)(volume) )
FC_REFLECT( bts::blockchain::order_record, (balance)(limit_price)(last_update) )
FC_REFLECT( bts::blockchain::collateral_record, (collateral_balance)(payoff_balance)(interest_rate)(expiration) )
FC_REFLECT( bts::blockchain::market_order, (type)(market_index)(state)(collateral)(interest_rate)(expiration) )
FC_REFLECT_TYPENAME( std::vector<bts::blockchain::market_transaction> )
FC_REFLECT_TYPENAME( bts::blockchain::market_history_key::time_granularity_enum ) // http://en.wikipedia.org/wiki/Voodoo_programminqg
FC_REFLECT( bts::blockchain::market_transaction,
            (bid_owner)
            (ask_owner)
            (bid_price)
            (ask_price)
            (bid_paid)
            (bid_received)
            (short_collateral)
            (ask_paid)
            (ask_received)
            (returned_collateral)
            (bid_type)
            (ask_type)
            (fees_collected)
          )
FC_REFLECT_DERIVED( bts::blockchain::order_history_record, (bts::blockchain::market_transaction), (timestamp) )
