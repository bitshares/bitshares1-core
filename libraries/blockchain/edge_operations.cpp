#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/edge_operations.hpp>
#include <bts/blockchain/edge_record.hpp>
#include <bts/blockchain/exceptions.hpp>

namespace bts { namespace blockchain {

    // If ID is zero, make a new object (get a new ID)
    // if ID is positive, update the existing object
    void set_edge_operation::evaluate( transaction_evaluation_state& eval_state )
    { try {

    } FC_CAPTURE_AND_RETHROW( (*this)(eval_state) ) }

}} // bts::blockchain
