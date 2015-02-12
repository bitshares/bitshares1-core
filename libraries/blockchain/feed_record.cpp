#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/feed_record.hpp>

namespace bts { namespace blockchain {

    void feed_record::sanity_check( const chain_interface& db )const
    { try {
        FC_ASSERT( db.lookup<asset_record>( index.quote_id ).valid() );
        FC_ASSERT( db.lookup<account_record>( index.delegate_id ).valid() );
        FC_ASSERT( value.quote_asset_id == index.quote_id );
        FC_ASSERT( value.base_asset_id == 0 );
    } FC_CAPTURE_AND_RETHROW( (*this) ) }

    ofeed_record feed_record::lookup( const chain_interface& db, const feed_index index )
    { try {
        return db.feed_lookup_by_index( index );
    } FC_CAPTURE_AND_RETHROW( (index) ) }

    void feed_record::store( chain_interface& db, const feed_index index, const feed_record& record )
    { try {
        db.feed_insert_into_index_map( index, record );
    } FC_CAPTURE_AND_RETHROW( (index)(record) ) }

    void feed_record::remove( chain_interface& db, const feed_index index )
    { try {
        const ofeed_record prev_record = db.lookup<feed_record>( index );
        if( prev_record.valid() )
            db.feed_erase_from_index_map( index );
    } FC_CAPTURE_AND_RETHROW( (index) ) }

} } // bts::blockchain
