#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/status_record.hpp>

namespace bts { namespace blockchain {

    void status_record::sanity_check( const chain_interface& db )const
    { try {
        FC_ASSERT( index.quote_id > index.base_id );
        FC_ASSERT( db.lookup<asset_record>( index.quote_id ).valid() );
        FC_ASSERT( db.lookup<asset_record>( index.base_id ).valid() );
        if( current_feed_price.valid() )
        {
            FC_ASSERT( last_valid_feed_price.valid() );
            FC_ASSERT( current_feed_price->quote_asset_id == index.quote_id );
            FC_ASSERT( current_feed_price->base_asset_id == index.base_id );
        }
        if( last_valid_feed_price.valid() )
        {
            FC_ASSERT( last_valid_feed_price->quote_asset_id == index.quote_id );
            FC_ASSERT( last_valid_feed_price->base_asset_id == index.base_id );
        }
    } FC_CAPTURE_AND_RETHROW( (*this) ) }

    ostatus_record status_record::lookup( const chain_interface& db, const status_index index )
    { try {
        return db.status_lookup_by_index( index );
    } FC_CAPTURE_AND_RETHROW( (index) ) }

    void status_record::store( chain_interface& db, const status_index index, const status_record& record )
    { try {
        db.status_insert_into_index_map( index, record );
    } FC_CAPTURE_AND_RETHROW( (index)(record) ) }

    void status_record::remove( chain_interface& db, const status_index index )
    { try {
        const ostatus_record prev_record = db.lookup<status_record>( index );
        if( prev_record.valid() )
            db.status_erase_from_index_map( index );
    } FC_CAPTURE_AND_RETHROW( (index) ) }

} } // bts::blockchain
