#pragma once

#include <bts/blockchain/operations.hpp>

namespace bts { namespace blockchain {

struct define_slate_operation
{
    static const operation_type_enum type;

    vector<signed_int> slate;

    void evaluate( transaction_evaluation_state& eval_state )const;
};

} } // bts::blockchain

FC_REFLECT( bts::blockchain::define_slate_operation, (slate) )
