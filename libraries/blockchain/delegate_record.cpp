#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/delegate_record.hpp>

namespace bts { namespace blockchain {

    void delegate_record::sanity_check( const chain_interface& db )const
    { try {
        FC_ASSERT( id > 0 );
        FC_ASSERT( votes_for >= 0 );
        FC_ASSERT( pay_rate <= 100 );
        FC_ASSERT( pay_balance >= 0 );
    } FC_CAPTURE_AND_RETHROW( (*this) ) }

    odelegate_record delegate_record::lookup( const chain_interface& db, const delegate_id_type id )
    { try {
        return db.delegate_lookup_by_id( id );
    } FC_CAPTURE_AND_RETHROW( (id) ) }

    void delegate_record::store( chain_interface& db, const delegate_id_type id, const delegate_record& record )
    { try {
        const odelegate_record prev_record = db.lookup<delegate_record>( id );
        if( prev_record.valid() )
        {
            if( !prev_record->retracted )
            {
                if( record.retracted || prev_record->votes_for != record.votes_for )
                    db.delegate_erase_from_vote_set( vote_del( prev_record->votes_for, prev_record->id ) );
            }
        }

        db.delegate_insert_into_id_map( id, record );
        if( !record.retracted )
            db.delegate_insert_into_vote_set( vote_del( record.votes_for, id ) );
    } FC_CAPTURE_AND_RETHROW( (id)(record) ) }

    void delegate_record::remove( chain_interface& db, const delegate_id_type id )
    { try {
        const odelegate_record prev_record = db.lookup<delegate_record>( id );
        if( prev_record.valid() )
        {
            db.delegate_erase_from_id_map( id );
            if( !prev_record->retracted )
                db.delegate_erase_from_vote_set( vote_del( prev_record->votes_for, prev_record->id ) );
        }
    } FC_CAPTURE_AND_RETHROW( (id) ) }

} } // bts::blockchain
