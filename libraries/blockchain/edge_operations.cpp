#include <bts/blockchain/edge_operations.hpp>
#include <bts/blockchain/edge_record.hpp>
#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/pending_chain_state.hpp>
#include <fc/io/raw_variant.hpp>

namespace bts { namespace blockchain {

    // If ID is zero, make a new object (get a new ID)
    // if ID is positive, update the existing object
    void set_edge_operation::evaluate( transaction_evaluation_state& eval_state )const
    { try {
        ilog("@n set_edge operation with edge: ${e}", ("e", this->edge));
        auto edge_data = edge.as<edge_record>();

        ilog("@n edge's properties:");
        ilog("@n   _id: ${id}, type: ${type}, short_id: ${short}",
                ("id", edge._id)("type", edge.type())("short", edge.short_id()));

        // Validate the contents in the proposed edge
        auto from = eval_state._current_state->get_object_record( edge_data.from );
        auto to = eval_state._current_state->get_object_record( edge_data.from );
        FC_ASSERT( from.valid(), "That 'from' object does not exist!" );
        FC_ASSERT( to.valid(), "That 'to' object does not exist!" );

        auto owners = eval_state._current_state->get_object_condition( *from );
        if( NOT eval_state.check_multisig( owners ) )
            FC_CAPTURE_AND_THROW( missing_signature, ( owners ) );

        ilog("@n getting edge");
        auto existing_edge = eval_state._current_state->get_edge( edge_data.from, edge_data.to, edge_data.name );

        if( !existing_edge.valid() )
        {
            ilog("@n edge exists, synchronizing object IDs");
            existing_edge = object_record(edge_data, edge_object, 0);
            auto next_id = eval_state._current_state->new_object_id( edge_object );
            existing_edge->_id = next_id;
        }
        existing_edge->set_data( edge_data );
        ilog("@n storing the edge");
        eval_state._current_state->store_edge_record( *existing_edge );
    } FC_CAPTURE_AND_RETHROW( (*this)(eval_state) ) }

}} // bts::blockchain
