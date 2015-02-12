#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/transaction_record.hpp>

namespace bts { namespace blockchain {

    void transaction_record::sanity_check( const chain_interface& db )const
    { try {
        FC_ASSERT( !trx.reserved.valid() );
        FC_ASSERT( !trx.operations.empty() );
        FC_ASSERT( !trx.signatures.empty() );
    } FC_CAPTURE_AND_RETHROW( (*this) ) }

    otransaction_record transaction_record::lookup( const chain_interface& db, const transaction_id_type& id )
    { try {
        return db.transaction_lookup_by_id( id );
    } FC_CAPTURE_AND_RETHROW( (id) ) }

    void transaction_record::store( chain_interface& db, const transaction_id_type& id, const transaction_record& record )
    { try {
        db.transaction_insert_into_id_map( id, record );
        db.transaction_insert_into_unique_set( record.trx );
    } FC_CAPTURE_AND_RETHROW( (id)(record) ) }

    void transaction_record::remove( chain_interface& db, const transaction_id_type& id )
    { try {
        const otransaction_record prev_record = db.lookup<transaction_record>( id );
        if( prev_record.valid() )
        {
            db.transaction_erase_from_id_map( id );
            db.transaction_erase_from_unique_set( prev_record->trx );
        }
    } FC_CAPTURE_AND_RETHROW( (id) ) }

} } // bts::blockchain
