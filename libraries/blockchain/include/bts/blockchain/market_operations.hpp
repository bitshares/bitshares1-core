#pragma once

#include <bts/blockchain/market_records.hpp>
#include <bts/blockchain/operations.hpp>

namespace bts { namespace blockchain {

   #include "market_operations_v1.hpp"

   struct bid_operation
   {
        static const operation_type_enum type;
        bid_operation():amount(0){}

        /** bid amount is in the quote unit */
        asset            get_amount()const { return asset( amount, bid_index.order_price.quote_asset_id ); }
        share_type       amount;
        market_index_key bid_index;

        void evaluate( transaction_evaluation_state& eval_state )const;
   };

   struct ask_operation
   {
        static const operation_type_enum type;
        ask_operation():amount(0){}

        asset             get_amount()const { return asset( amount, ask_index.order_price.base_asset_id ); }
        share_type        amount;
        market_index_key  ask_index;

        void evaluate( transaction_evaluation_state& eval_state )const;
        void evaluate_v1( transaction_evaluation_state& eval_state )const;
   };

   struct relative_bid_operation
   {
        static const operation_type_enum type;
        relative_bid_operation():amount(0){}

        /** bid amount is in the quote unit */
        asset            get_amount()const { return asset( amount, bid_index.order_price.quote_asset_id ); }
        share_type       amount;
        market_index_key bid_index;
        optional<price>  limit_price;

        void evaluate( transaction_evaluation_state& eval_state )const;
   };

   struct relative_ask_operation
   {
        static const operation_type_enum type;
        relative_ask_operation():amount(0){}

        asset             get_amount()const { return asset( amount, ask_index.order_price.base_asset_id ); }
        share_type        amount;
        market_index_key  ask_index;
        optional<price>   limit_price;

        void evaluate( transaction_evaluation_state& eval_state )const;
   };


   struct short_operation
   {
        static const operation_type_enum type;
        short_operation():amount(0){}

        asset            get_amount()const { return asset( amount, short_index.order_price.base_asset_id ); }

        share_type             amount;
        market_index_key_ext   short_index;

        void evaluate( transaction_evaluation_state& eval_state )const;
        void evaluate_v1( transaction_evaluation_state& eval_state )const;
   };

   struct cover_operation
   {
        static const operation_type_enum type;
        cover_operation():amount(0){}
        cover_operation( share_type a, const market_index_key& idx )
        :amount(a),cover_index(idx){}

        asset               get_amount()const { return asset( amount, cover_index.order_price.quote_asset_id ); }
        share_type          amount;
        market_index_key    cover_index;
        fc::optional<price> new_cover_price;

        void evaluate( transaction_evaluation_state& eval_state )const;
        void evaluate_v4( transaction_evaluation_state& eval_state )const;
        void evaluate_v3( transaction_evaluation_state& eval_state )const;
        void evaluate_v2( transaction_evaluation_state& eval_state )const;
        void evaluate_v1( transaction_evaluation_state& eval_state )const;
   };

   /**
    *  Increases the collateral and lowers call price *if* the
    *  new minimal call price is higher than the old call price.
    */
   struct add_collateral_operation
   {
        static const operation_type_enum type;
        add_collateral_operation(share_type amount = 0, market_index_key cover_index = market_index_key())
          : amount(amount), cover_index(cover_index){}

        asset            get_amount()const { return asset( amount, cover_index.order_price.base_asset_id ); }
        share_type       amount;
        market_index_key cover_index;

        void evaluate( transaction_evaluation_state& eval_state )const;
        void evaluate_v1( transaction_evaluation_state& eval_state )const;
   };

   /**
    *  The call price can be set to any price above the minimum call
    *  price which is defined as 50% of the collateral being required
    *  to cover 100% of the debt.  
    *
    *  This can be used to protect the short as a "stop loss" and to
    *  allow the short to exit their position without having to keep
    *  extra BTS on the side to buy USD to cover the order.
    */
   struct update_call_price_operation
   {
      static const operation_type_enum type;

      market_index_key cover_index;
      price            new_call_price;

      void evaluate( transaction_evaluation_state& eval_state )const;
   };

} } // bts::blockchain

FC_REFLECT( bts::blockchain::bid_operation,               (amount)(bid_index))
FC_REFLECT( bts::blockchain::ask_operation,               (amount)(ask_index))
FC_REFLECT( bts::blockchain::relative_bid_operation,      (amount)(bid_index)(limit_price))
FC_REFLECT( bts::blockchain::relative_ask_operation,      (amount)(ask_index)(limit_price))
FC_REFLECT( bts::blockchain::short_operation,             (amount)(short_index) )
FC_REFLECT( bts::blockchain::short_operation_v1,          (amount)(short_index) )
FC_REFLECT( bts::blockchain::cover_operation,             (amount)(cover_index)(new_cover_price) )
FC_REFLECT( bts::blockchain::add_collateral_operation,    (amount)(cover_index))
FC_REFLECT( bts::blockchain::update_call_price_operation, (cover_index)(new_call_price))
