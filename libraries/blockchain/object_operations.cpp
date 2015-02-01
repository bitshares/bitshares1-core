#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/object_operations.hpp>
#include <bts/blockchain/object_record.hpp>
#include <bts/blockchain/pending_chain_state.hpp>

namespace bts { namespace blockchain {

    // If ID is zero, make a new object (get a new ID)
    // if ID is negative, look in evaluation stack
    // if ID is positive, update the existing object
    void set_object_operation::evaluate( transaction_evaluation_state& eval_state )const
    { try {
        ilog("@n setting base object object");
        FC_ASSERT( this->obj.type() == obj_type::base_object,
                "you can only use this interface for base objects right now!");
        auto existing = eval_state._current_state->get_object_record( this->obj._id );
        if( existing.valid() )
        {
            ilog("@n object exists: ${e}", ("e", *existing));
            object_record updated_obj = object_record( this->obj );
            auto owners = eval_state._current_state->get_object_condition( *existing );
            ilog("@n     the multisig condition is: ${owners}", ("owners", owners) );
            if( NOT eval_state.check_multisig( owners ) )
                FC_CAPTURE_AND_THROW( missing_signature, (owners) );
            eval_state._current_state->store_object_record( updated_obj );
            ilog("@n storing object:  ");
            ilog("@n   _id: ${id}, type: ${type}, short_id: ${short}",
                ("id", updated_obj._id)("type", updated_obj.type())("short", updated_obj.short_id()));

        }
        else
        {
            ilog("@n No such object, getting a new ID and storing it");
            auto next_id = eval_state._current_state->new_object_id( base_object );
            object_record new_obj = object_record( this->obj );
            new_obj.set_id( this->obj.type(), next_id );
            new_obj.owner_object = new_obj._id;
            //auto owners = eval_state._current_state->get_object_condition( new_obj );
            //ilog("@n     the multisig condition is: ${owners}", ("owners", owners) );
            //if( NOT eval_state.check_multisig( owners ) )
            //    FC_CAPTURE_AND_THROW( missing_signature, ( owners ) );
            ilog("@n storing object:  ");
            ilog("@n   _id: ${id}, type: ${type}, short_id: ${short}",
                ("id", new_obj._id)("type", new_obj.type())("short", new_obj.short_id()));

            eval_state._current_state->store_object_record( new_obj );
        }

    } FC_CAPTURE_AND_RETHROW( (*this)(eval_state) ) }

}} // bts::blockchain
