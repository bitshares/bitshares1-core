#pragma once
#include <bts/blockchain/operations.hpp>

namespace bts { namespace blockchain {

    struct site_create_operation
    {
        static const operation_type_enum type;

        void evaluate( transaction_evaluation_state& eval_state );
    };

    struct site_update_operation
    {
        static const operation_type_enum type;

        void evaluate( transaction_evaluation_state& eval_state );
    };


}} // bts::blockchain

