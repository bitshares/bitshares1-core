#pragma once
#include <bts/blockchain/operations.hpp>
#include <bts/blockchain/market_records.hpp>

namespace bts { namespace blockchain {

   struct bid_operation
   {
        static const operation_type_enum type; 
        bid_operation():amount(0){}

        /** bid amount is in the quote unit */
        asset            get_amount()const { return asset( amount, bid_index.order_price.quote_asset_id ); }
        share_type       amount;
        market_index_key bid_index;
        name_id_type     delegate_id;

        void evaluate( transaction_evaluation_state& eval_state );
   };

   struct ask_operation
   {
        static const operation_type_enum type; 
        ask_operation():amount(0){}

        asset        get_amount()const { return asset( amount, ask_index.order_price.base_asset_id ); }
        share_type   amount;
        market_index_key ask_index;
        name_id_type delegate_id;

        void evaluate( transaction_evaluation_state& eval_state );
   };

   struct short_operation
   {
        static const operation_type_enum type; 
        short_operation():amount(0){}

        asset        get_amount()const { return asset( amount, 0 ); }
        share_type   amount;
        market_index_key short_index;
        name_id_type delegate_id;

        void evaluate( transaction_evaluation_state& eval_state );
   };
   
   struct cover_operation
   {
        static const operation_type_enum type; 
        cover_operation():amount(0){}

        asset            get_amount()const { return asset( amount, cover_index.order_price.base_asset_id ); }
        share_type       amount;
        market_index_key cover_index;
        name_id_type     delegate_id;

        void evaluate( transaction_evaluation_state& eval_state );
   };

   struct add_collateral_operation
   {
        static const operation_type_enum type; 
        add_collateral_operation():amount(0){}

        share_type   amount;
        address      owner;
        name_id_type delegate_id;

        void evaluate( transaction_evaluation_state& eval_state );
   };

   struct remove_collateral_operation
   {
        static const operation_type_enum type; 
        remove_collateral_operation():amount(0){}

        share_type   amount;
        address      owner;
        name_id_type delegate_id;

        void evaluate( transaction_evaluation_state& eval_state );
   };

} } // bts::blockchain

FC_REFLECT( bts::blockchain::bid_operation, (amount)(bid_index)(delegate_id) )
FC_REFLECT( bts::blockchain::ask_operation, (amount)(ask_index)(delegate_id) )
FC_REFLECT( bts::blockchain::short_operation, (amount)(short_index)(delegate_id) )
FC_REFLECT( bts::blockchain::cover_operation, (amount)(cover_index)(delegate_id) )
FC_REFLECT( bts::blockchain::add_collateral_operation, (amount)(owner)(delegate_id) )
FC_REFLECT( bts::blockchain::remove_collateral_operation, (amount)(owner)(delegate_id) )
