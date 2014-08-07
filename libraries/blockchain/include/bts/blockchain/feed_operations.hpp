#pragma once
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/operations.hpp>

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

  struct feed_entry
  {
      string           asset_symbol;
      string           delegate_name;
      double           price;
      time_point_sec   last_update;
  };

  /**
   *  A feed can be published by any active delegate.  By default a new
   *  feed is created for each market pegged asset and that feed is expected
   *  to be a price object.
   */
  struct update_feed_operation
  {
      static const operation_type_enum type; 
      feed_index   feed;
      fc::variant  value;

      void evaluate( transaction_evaluation_state& eval_state );
  };

  typedef fc::optional<feed_record> ofeed_record;

} } // bts::blockchain

FC_REFLECT( bts::blockchain::feed_index, (feed_id)(delegate_id) )
FC_REFLECT( bts::blockchain::feed_record, (feed)(value)(last_update) )
FC_REFLECT( bts::blockchain::feed_entry, (asset_symbol)(delegate_name)(price)(last_update) );
FC_REFLECT( bts::blockchain::update_feed_operation, (feed)(value) )
