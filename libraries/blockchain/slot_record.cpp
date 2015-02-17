#include <bts/blockchain/slot_record.hpp>
#include <bts/blockchain/chain_interface.hpp>

namespace bts { namespace blockchain {

void slot_record::sanity_check( const chain_interface& db )const
{ try {
    FC_ASSERT( db.lookup<account_record>( index.delegate_id ).valid() );
} FC_CAPTURE_AND_RETHROW( (*this) ) }

oslot_record slot_record::lookup( const chain_interface& db, const slot_index index )
{ try {
    return db.slot_lookup_by_index( index );
} FC_CAPTURE_AND_RETHROW( (index) ) }

oslot_record slot_record::lookup( const chain_interface& db, const time_point_sec timestamp )
{ try {
    if( !db.get_statistics_enabled() ) return oslot_record();
    return db.slot_lookup_by_timestamp( timestamp );
} FC_CAPTURE_AND_RETHROW( (timestamp) ) }

void slot_record::store( chain_interface& db, const slot_index index, const slot_record& record )
{ try {
    if( !db.get_statistics_enabled() ) return;
    const oslot_record prev_record = db.lookup<slot_record>( index );
    if( prev_record.valid() )
    {
        if( prev_record->index.timestamp != index.timestamp )
            db.slot_erase_from_timestamp_map( prev_record->index.timestamp );
    }

    db.slot_insert_into_index_map( index, record );
    db.slot_insert_into_timestamp_map( index.timestamp, index.delegate_id );
} FC_CAPTURE_AND_RETHROW( (index)(record) ) }

void slot_record::remove( chain_interface& db, const slot_index index )
{ try {
    if( !db.get_statistics_enabled() ) return;
    const oslot_record prev_record = db.lookup<slot_record>( index );
    if( prev_record.valid() )
    {
        db.slot_erase_from_index_map( index );
        db.slot_erase_from_timestamp_map( prev_record->index.timestamp );
    }
} FC_CAPTURE_AND_RETHROW( (index) ) }

} } // bts::blockchain
