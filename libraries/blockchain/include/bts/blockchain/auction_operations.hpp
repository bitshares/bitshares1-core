#pragma once
#include <bts/blockchain/operations.hpp>
#include <bts/blockchain/object_record.hpp>

namespace bts { namespace blockchain {

   struct auction_start_operation : operation
   {
      static const operation_type_enum type;

      void evaluate( transaction_evaluation_state& eval_state );
   };

   struct auction_bid_operation : operation
   {
      static const operation_type_enum type;

      void evaluate( transaction_evaluation_state& eval_state );
   };


}} // bts::blockchain

