#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/feed_record.hpp>

namespace bts { namespace blockchain {

    const feed_db_interface& feed_record::db_interface( const chain_interface& db )
    { try {
        return db._feed_db_interface;
    } FC_CAPTURE_AND_RETHROW() }

    ofeed_record feed_db_interface::lookup( const feed_index index )const
    { try {
        return lookup_by_index( index );
    } FC_CAPTURE_AND_RETHROW( (index) ) }

    void feed_db_interface::store( const feed_index index, const feed_record& record )const
    { try {
        insert_into_index_map( index, record );
    } FC_CAPTURE_AND_RETHROW( (index)(record) ) }

    void feed_db_interface::remove( const feed_index index )const
    { try {
        const ofeed_record prev_record = lookup( index );
        if( prev_record.valid() )
            erase_from_index_map( index );
    } FC_CAPTURE_AND_RETHROW( (index) ) }

} } // bts::blockchain
