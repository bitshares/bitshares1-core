#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/object_operations.hpp>
#include <bts/blockchain/object_record.hpp>
#include <bts/blockchain/exceptions.hpp>

namespace bts { namespace blockchain {

    // If ID is zero, make a new object (get a new ID)
    // if ID is negative, look in evaluation stack
    // if ID is positive, update the existing object
    void set_object_operation::evaluate( transaction_evaluation_state& eval_state )
    { try {
        ilog("@n setting object");
        object_record obj;
        auto oobj = eval_state._current_state->get_object_record( this->obj._id );
        if( oobj.valid() )
        {
            ilog("@n object exists");
            obj = object_record( *oobj );
            auto owners = eval_state._current_state->get_object_condition( obj );
            if( NOT eval_state.check_multisig( owners ) )
                FC_CAPTURE_AND_THROW( missing_signature, (owners) );
        }
        else
        {
            ilog("@n No such object, getting a new ID and checking permissions: ");
            FC_ASSERT( this->obj.type() == base_object, "Must use this to set a base object!" );
            auto next_id = eval_state._current_state->new_object_id( base_object );
            obj = object_record( this->obj );
            obj.set_id( this->obj.type(), next_id );
            auto owners = eval_state._current_state->get_object_condition( obj );
            if( NOT eval_state.check_multisig( owners ) )
                FC_CAPTURE_AND_THROW( missing_signature, ( owners ) );
        }
        ilog("@n storing object:  ");
        ilog("@n   _id: ${id}, type: ${type}, short_id: ${short}",
                ("id", obj._id)("type", obj.type())("short", obj.short_id()));

        eval_state._current_state->store_object_record( obj );
    } FC_CAPTURE_AND_RETHROW( (*this)(eval_state) ) }

}} // bts::blockchain
