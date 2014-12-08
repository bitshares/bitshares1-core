#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/edge_operations.hpp>
#include <bts/blockchain/edge_record.hpp>
#include <bts/blockchain/exceptions.hpp>

namespace bts { namespace blockchain {

    // If ID is zero, make a new object (get a new ID)
    // if ID is positive, update the existing object
    void set_edge_operation::evaluate( transaction_evaluation_state& eval_state )
    { try {
        // Validate the contents in the proposed edge
        auto from = eval_state._current_state->get_object_record( this->edge.from );
        auto to = eval_state._current_state->get_object_record( this->edge.from );
        FC_ASSERT( from.valid(), "That 'from' object does not exist!" );
        FC_ASSERT( to.valid(), "That 'to' object does not exist!" );

        auto owners = eval_state._current_state->get_object_owners( *from );
        if( NOT eval_state.check_multisig( owners ) )
            FC_CAPTURE_AND_THROW( missing_signature, ( owners ) );

        auto edge = eval_state._current_state->get_edge( this->edge.from, this->edge.to, this->edge.name );

        if( edge.valid() )
        {
            this->edge._id = edge->_id;
        }
        else
        {
            auto next_id = eval_state._current_state->new_object_id( edge_object );
            this->edge.set_id( edge_object, next_id );
        }
        eval_state._current_state->store_object_record( object_record(edge) );
    } FC_CAPTURE_AND_RETHROW( (*this)(eval_state) ) }

}} // bts::blockchain
