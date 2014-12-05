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
        object_record obj;

        if( this->id < 0 )
        {
            FC_ASSERT(! "unimplemented: set_object with negative id" );
        }
        else if( this->id == 0 )
        {
            ilog("@1 Object type: ${t}", ("t", this->obj.type()));
            auto next_id = eval_state._current_state->new_object_id(this->obj.type());
            obj = this->obj;
            obj.set_id( this->obj.type(), next_id );
            ilog("@2 Object type: ${t}", ("t", this->obj.type()));
            switch( obj.type() )
            {
                case( base_object ):
                case( edge_object ):
                {
                    auto owners = eval_state._current_state->get_object_owners( obj );
                    if( NOT eval_state.check_multisig( owners ) )
                        FC_CAPTURE_AND_THROW( missing_signature, ( owners ) );
                    break;
                }
                case( account_object ):
                case( asset_object ):
                    FC_ASSERT(!"Storing legacy objects via object interface is not supported yet.");
                default:
                    FC_ASSERT(!"Unsupported object type!");
            }
        }
        else // id > 0
        {
            auto real_id = object_record( this->obj.type(), this->id )._id;
            auto oobj = eval_state._current_state->get_object_record( real_id );
            FC_ASSERT( oobj.valid(), "No object with that ID.");
            obj = *oobj;
            auto owners = eval_state._current_state->get_object_owners( obj );
            switch( obj.type() )
            {
                case( base_object ):
                case( edge_object ):
                {
                    if( NOT eval_state.check_multisig( owners ) )
                        FC_CAPTURE_AND_THROW( missing_signature, (owners) );
                }
                case( account_object ):
                case( asset_object ):
                default:
                    FC_ASSERT(!"Unimplemented!");
            }
        }
        eval_state._current_state->store_object_record( obj );
    } FC_CAPTURE_AND_RETHROW( (*this)(eval_state) ) }

}} // bts::blockchain
