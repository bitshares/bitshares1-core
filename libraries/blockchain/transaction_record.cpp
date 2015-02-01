#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/transaction_record.hpp>
#include <fc/exception/exception.hpp>

namespace bts { namespace blockchain {

    const transaction_db_interface& transaction_record::db_interface( const chain_interface& db )
    { try {
        return db._transaction_db_interface;
    } FC_CAPTURE_AND_RETHROW() }

    otransaction_record transaction_db_interface::lookup( const transaction_id_type& id )const
    { try {
        return lookup_by_id( id );
    } FC_CAPTURE_AND_RETHROW( (id) ) }

    void transaction_db_interface::store( const transaction_id_type& id, const transaction_record& record )const
    { try {
        insert_into_id_map( id, record );
        insert_into_unique_set( record.trx );
    } FC_CAPTURE_AND_RETHROW( (id)(record) ) }

    void transaction_db_interface::remove( const transaction_id_type& id )const
    { try {
        const otransaction_record prev_record = lookup( id );
        if( prev_record.valid() )
        {
            erase_from_id_map( id );
            erase_from_unique_set( prev_record->trx );
        }
    } FC_CAPTURE_AND_RETHROW( (id) ) }

} } // bts::blockchain
