#pragma once

#include <bts/blockchain/types.hpp>

namespace bts { namespace blockchain {

  struct feed_index
  {
      feed_id_type      feed_id;
      account_id_type   delegate_id;

      friend bool operator < ( const feed_index& a, const feed_index& b )
      {
         return a.feed_id < b.feed_id ? true :
                a.feed_id != b.feed_id ? false:
                a.delegate_id < b.delegate_id;
      }
      friend bool operator == ( const feed_index& a, const feed_index& b )
      {
         return a.feed_id == b.feed_id  && a.delegate_id == b.delegate_id;
      }
  };

  struct feed_record
  {
      bool        is_null()const{ return value.is_null(); }
      feed_record make_null()const { return feed_record{feed,variant()}; }

      feed_index       feed;
      variant          value;
      time_point_sec   last_update;
  };
  typedef fc::optional<feed_record> ofeed_record;

  struct feed_entry
  {
      string                         delegate_name;
      double                         price;
      time_point_sec                 last_update;
      fc::optional<string>           asset_symbol;
      fc::optional<double>           median_price;
  };

} } // bts::blockchain

FC_REFLECT( bts::blockchain::feed_index, (feed_id)(delegate_id) )
FC_REFLECT( bts::blockchain::feed_record, (feed)(value)(last_update) )
FC_REFLECT( bts::blockchain::feed_entry, (delegate_name)(price)(last_update)(asset_symbol)(median_price) );
