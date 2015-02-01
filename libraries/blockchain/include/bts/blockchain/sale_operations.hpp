#pragma once
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/asset.hpp>
#include <bts/blockchain/operations.hpp>

namespace bts { namespace blockchain {

    struct make_sale_operation
    {
        static const operation_type_enum type;

        object_id_type   object_to_sell;
        asset            price;

        void evaluate( transaction_evaluation_state& eval_state )const;
    };

    struct buy_sale_operation
    {
        static const operation_type_enum type;

        object_id_type    sale_id;
        asset             offer;
        object_id_type    new_owner;

        void evaluate( transaction_evaluation_state& eval_state )const;
    };


}} // bts::blockchain

FC_REFLECT( bts::blockchain::make_sale_operation, (object_to_sell)(price) );
FC_REFLECT( bts::blockchain::buy_sale_operation, (sale_id)(offer)(new_owner) );
