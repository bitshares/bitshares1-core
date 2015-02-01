#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/pending_chain_state.hpp>
#include <bts/blockchain/slate_operations.hpp>

#include <bts/blockchain/fork_blocks.hpp>

namespace bts { namespace blockchain {

void define_slate_operation::evaluate( transaction_evaluation_state& eval_state )
{ try {
    if( this->slate.size() > BTS_BLOCKCHAIN_MAX_SLATE_SIZE )
        FC_CAPTURE_AND_THROW( too_may_delegates_in_slate, (slate.size()) );

    slate_record record;
    for( const signed_int id : this->slate )
    {
#ifndef WIN32
#warning [SOFTFORK] Remove this check after BTS_V0_6_2_FORK_BLOCK_NUM has passed
#endif
        if( eval_state._current_state->get_head_block_num() < BTS_V0_6_2_FORK_BLOCK_NUM )
            eval_state.verify_delegate_id( id );

        FC_ASSERT( id >= 0 );
        record.slate.insert( id );
    }

    if( eval_state._current_state->get_head_block_num() < BTS_V0_6_3_FORK_BLOCK_NUM )
    {
        const slate_id_type slate_id = slate_record::id_v1( this->slate );
        const oslate_record current_slate = eval_state._current_state->get_slate_record( slate_id );
        if( !current_slate.valid() )
        {
            if( record.slate.size() < this->slate.size() )
                record.duplicate_slate = this->slate;

            eval_state._current_state->store( slate_id, record );
            record.duplicate_slate.clear();
        }
    }

    const slate_id_type slate_id = record.id();
    const oslate_record current_slate = eval_state._current_state->get_slate_record( slate_id );
    if( current_slate.valid() )
        return;

    eval_state._current_state->store_slate_record( record );
} FC_CAPTURE_AND_RETHROW( (*this) ) }

} } // bts::blockchain
