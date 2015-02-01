#include <bts/blockchain/exceptions.hpp>
#include <bts/blockchain/pending_chain_state.hpp>
#include <bts/blockchain/slate_operations.hpp>

namespace bts { namespace blockchain {

void define_slate_operation::evaluate( transaction_evaluation_state& eval_state )const
{ try {
    if( this->slate.size() > BTS_BLOCKCHAIN_MAX_SLATE_SIZE )
        FC_CAPTURE_AND_THROW( too_may_delegates_in_slate, (slate.size()) );

    slate_record record;
    for( const signed_int id : this->slate )
    {
        FC_ASSERT( id >= 0 );
        record.slate.insert( id );
    }

    const slate_id_type slate_id = record.id();
    const oslate_record current_slate = eval_state._current_state->get_slate_record( slate_id );
    if( current_slate.valid() )
        return;

    eval_state._current_state->store_slate_record( record );
} FC_CAPTURE_AND_RETHROW( (*this) ) }

} } // bts::blockchain
