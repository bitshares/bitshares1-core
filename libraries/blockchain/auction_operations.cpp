#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/auction_operations.hpp>
#include <bts/blockchain/exceptions.hpp>

namespace bts { namespace blockchain {

    void auction_start_operation::evaluate( transaction_evaluation_state& eval_state ) 
    {
        FC_ASSERT(!"unimplemented");
    }

    void auction_bid_operation::evaluate( transaction_evaluation_state& eval_state ) 
    {
        FC_ASSERT(!"unimplemented");
    }


}} //bts::blockchain
