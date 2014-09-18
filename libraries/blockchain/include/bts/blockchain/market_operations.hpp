#pragma once

#include <bts/blockchain/market_records.hpp>
#include <bts/blockchain/operations.hpp>

namespace bts { namespace blockchain {

   struct bid_operation
   {
        static const operation_type_enum type;
        bid_operation():amount(0){}

        /** bid amount is in the quote unit */
        asset            get_amount()const { return asset( amount, bid_index.order_price.quote_asset_id ); }
        share_type       amount;
        market_index_key bid_index;

        void evaluate( transaction_evaluation_state& eval_state );
   };

   struct ask_operation
   {
        static const operation_type_enum type;
        ask_operation():amount(0){}

        asset             get_amount()const { return asset( amount, ask_index.order_price.base_asset_id ); }
        share_type        amount;
        market_index_key  ask_index;

        void evaluate( transaction_evaluation_state& eval_state );
   };

   struct short_operation_v1
   {
        static const operation_type_enum type;
        short_operation_v1():amount(0){}

        asset            get_amount()const { return asset( amount, 0 ); }

        share_type       amount;
        market_index_key short_index;

        void evaluate( transaction_evaluation_state& eval_state );
        void evaluate_v1( transaction_evaluation_state& eval_state );
   };

   struct short_operation_v2
   {
        static const operation_type_enum type;
        short_operation_v2():amount(0){}

        asset            get_amount()const { return asset( amount, 0 ); }

        share_type       amount;
        market_index_key short_index;
        optional<price>  short_price_limit;

        void evaluate( transaction_evaluation_state& eval_state );
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

        void evaluate( transaction_evaluation_state& eval_state );
        void evaluate_v1( transaction_evaluation_state& eval_state );
   };

   struct add_collateral_operation
   {
        static const operation_type_enum type;
        add_collateral_operation(share_type amount = 0, market_index_key cover_index = market_index_key())
          : amount(amount), cover_index(cover_index){}

        asset            get_amount()const { return asset( amount, cover_index.order_price.base_asset_id ); }
        share_type       amount;
        market_index_key cover_index;

        void evaluate( transaction_evaluation_state& eval_state );
   };

   struct remove_collateral_operation
   {
        static const operation_type_enum type;
        remove_collateral_operation():amount(0){}

        share_type   amount;
        address      owner;

        void evaluate( transaction_evaluation_state& eval_state );
   };

} } // bts::blockchain

FC_REFLECT( bts::blockchain::bid_operation, (amount)(bid_index))
FC_REFLECT( bts::blockchain::ask_operation, (amount)(ask_index))
FC_REFLECT( bts::blockchain::short_operation_v1, (amount)(short_index) )
FC_REFLECT( bts::blockchain::short_operation_v2, (amount)(short_index)(short_price_limit) )
FC_REFLECT( bts::blockchain::cover_operation, (amount)(cover_index)(new_cover_price) )
FC_REFLECT( bts::blockchain::add_collateral_operation, (amount)(cover_index))
FC_REFLECT( bts::blockchain::remove_collateral_operation, (amount)(owner))
