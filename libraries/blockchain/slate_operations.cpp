#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/pending_chain_state.hpp>
#include <bts/blockchain/slate_operations.hpp>

#include <bts/blockchain/fork_blocks.hpp>

namespace bts { namespace blockchain {

void define_slate_operation::evaluate( transaction_evaluation_state& eval_state )const
{ try {
    FC_ASSERT( !slate.empty() );
    if( this->slate.size() > BTS_BLOCKCHAIN_MAX_SLATE_SIZE )
        FC_CAPTURE_AND_THROW( too_may_delegates_in_slate, (slate.size()) );

    slate_record record;
    for( const signed_int id : this->slate )
    {
        if( id >= 0 )
        {
            const oaccount_record delegate_record = eval_state.pending_state()->get_account_record( id );
            FC_ASSERT( delegate_record.valid() && delegate_record->is_delegate() );
        }
        record.slate.insert( id );
    }

    if( eval_state.pending_state()->get_head_block_num() < BTS_V0_6_3_FORK_BLOCK_NUM )
    {
        const slate_id_type slate_id = slate_record::id_v1( this->slate );
        const oslate_record current_slate = eval_state.pending_state()->get_slate_record( slate_id );
        if( !current_slate.valid() )
        {
            if( record.slate.size() < this->slate.size() )
                record.duplicate_slate = this->slate;

            eval_state.pending_state()->store( slate_id, record );
            record.duplicate_slate.clear();
        }
    }

    const slate_id_type slate_id = record.id();
    const oslate_record current_slate = eval_state.pending_state()->get_slate_record( slate_id );
    if( current_slate.valid() )
    {
        FC_ASSERT( current_slate->slate == record.slate, "Slate ID collision!", ("current_slate",*current_slate)("new_slate",record) );
        return;
    }

    eval_state.pending_state()->store_slate_record( record );
} FC_CAPTURE_AND_RETHROW( (*this) ) }

} } // bts::blockchain
