#pragma once

#include <bts/blockchain/feed_record.hpp>
#include <bts/blockchain/operations.hpp>
#include <bts/blockchain/types.hpp>

namespace bts { namespace blockchain {

  struct update_feed_operation
  {
      static const operation_type_enum type;
      feed_index   index;
      fc::variant  value;

      void evaluate( transaction_evaluation_state& eval_state )const;
      void evaluate_v1( transaction_evaluation_state& eval_state )const;
  };

} } // bts::blockchain

FC_REFLECT( bts::blockchain::update_feed_operation, (index)(value) )
