#pragma once

#include <bts/blockchain/asset.hpp>
#include <bts/blockchain/operations.hpp>
#include <bts/blockchain/object_record.hpp>

namespace bts { namespace blockchain {

   struct auction_start_operation
   {
      static const operation_type_enum type;

      object_id_type              item_id;
      object_id_type              beneficiary;
      time_point_sec              expiration;
      asset                       min_price;
      asset                       buy_now_price;

      void evaluate( transaction_evaluation_state& eval_state )const;
   };

   struct auction_bid_operation
   {
      static const operation_type_enum type;

      object_id_type              auction_id;
      asset                       bid;
      object_id_type              new_owner;

      void evaluate( transaction_evaluation_state& eval_state )const;
   };

   struct user_auction_claim_operation
   {
      static const operation_type_enum type;

      object_id_type              auction_id;
      bool                        claim_balance;
      bool                        claim_object;

      void evaluate( transaction_evaluation_state& eval_state )const;
   };

   struct throttled_auction_claim_operation
   {
      static const operation_type_enum type;

      object_id_type              auction_id;
      bool                        claim_object;

      void evaluate( transaction_evaluation_state& eval_state )const;
   };

}} // bts::blockchain

FC_REFLECT( bts::blockchain::user_auction_claim_operation, (auction_id)(claim_balance)(claim_object) );
