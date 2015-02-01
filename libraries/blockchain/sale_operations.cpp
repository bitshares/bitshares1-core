#include <bts/blockchain/sale_operations.hpp>
#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/exceptions.hpp>

namespace bts { namespace blockchain {

    void make_sale_operation::evaluate( transaction_evaluation_state& eval_state )const
    {
        FC_ASSERT(false, "unimplemented");
    }

    void buy_sale_operation::evaluate( transaction_evaluation_state& eval_state )const
    {
        FC_ASSERT(false, "unimplemented");
    }


}} //bts::blockchain
