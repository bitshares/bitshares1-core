#include <bts/blockchain/slot_record.hpp>
#include <bts/blockchain/chain_interface.hpp>

namespace bts { namespace blockchain {

const slot_db_interface& slot_record::db_interface( const chain_interface& db )
{ try {
    return db._slot_db_interface;
} FC_CAPTURE_AND_RETHROW() }

oslot_record slot_db_interface::lookup( const slot_index index )const
{ try {
    return lookup_by_index( index );
} FC_CAPTURE_AND_RETHROW( (index) ) }

oslot_record slot_db_interface::lookup( const time_point_sec timestamp )const
{ try {
    return lookup_by_timestamp( timestamp );
} FC_CAPTURE_AND_RETHROW( (timestamp) ) }

void slot_db_interface::store( const slot_index index, const slot_record& record )const
{ try {
    const oslot_record prev_record = lookup( index );
    if( prev_record.valid() )
    {
        if( prev_record->index.timestamp != index.timestamp )
            erase_from_timestamp_map( prev_record->index.timestamp );
    }

    insert_into_index_map( index, record );
    insert_into_timestamp_map( index.timestamp, index.delegate_id );
} FC_CAPTURE_AND_RETHROW( (index)(record) ) }

void slot_db_interface::remove( const slot_index index )const
{ try {
    const oslot_record prev_record = lookup( index );
    if( prev_record.valid() )
    {
        erase_from_index_map( index );
        erase_from_timestamp_map( prev_record->index.timestamp );
    }
} FC_CAPTURE_AND_RETHROW( (index) ) }

} } // bts::blockchain
