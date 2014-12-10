#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/object_operations.hpp>
#include <bts/blockchain/object_record.hpp>
#include <bts/blockchain/exceptions.hpp>

#include <bts/blockchain/fork_blocks.hpp>

namespace bts { namespace blockchain {

    // If ID is zero, make a new object (get a new ID)
    // if ID is negative, look in evaluation stack
    // if ID is positive, update the existing object
    void set_object_operation::evaluate( transaction_evaluation_state& eval_state )
    { try {
#ifndef WIN32
#warning [SOFTFORK] Remove this check after BTS_V0_4_27_FORK_BLOCK_NUM has passed
#endif
        FC_ASSERT( eval_state._current_state->get_head_block_num() >= BTS_V0_4_27_FORK_BLOCK_NUM );

        object_record obj;
        auto oobj = eval_state._current_state->get_object_record( this->obj._id );
        if( oobj.valid() )
        {
            obj = *oobj;
            auto owners = eval_state._current_state->get_object_owners( obj );
            if( NOT eval_state.check_multisig( owners ) )
                FC_CAPTURE_AND_THROW( missing_signature, (owners) );
        }
        else
        {
            FC_ASSERT( this->obj.type() == base_object, "Can't use this to set a base object!" );
            auto next_id = eval_state._current_state->new_object_id( base_object );
            obj = this->obj;
            obj.set_id( this->obj.type(), next_id );
            auto owners = eval_state._current_state->get_object_owners( obj );
            if( NOT eval_state.check_multisig( owners ) )
                FC_CAPTURE_AND_THROW( missing_signature, ( owners ) );
        }
        eval_state._current_state->store_object_record( obj );
    } FC_CAPTURE_AND_RETHROW( (*this)(eval_state) ) }

}} // bts::blockchain
