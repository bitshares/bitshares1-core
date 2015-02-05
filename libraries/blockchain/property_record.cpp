#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/property_record.hpp>

namespace bts { namespace blockchain {

const property_db_interface& property_record::db_interface( const chain_interface& db )
{ try {
    return db._property_db_interface;
} FC_CAPTURE_AND_RETHROW() }

void property_record::sanity_check( const chain_interface& db )const
{ try {
    FC_ASSERT( !value.is_null() );
} FC_CAPTURE_AND_RETHROW( (*this) ) }

oproperty_record property_db_interface::lookup( const property_id_type id )const
{ try {
    return lookup_by_id( id );
} FC_CAPTURE_AND_RETHROW( (id) ) }

void property_db_interface::store( const property_id_type id, const property_record& record )const
{ try {
    insert_into_id_map( id, record );
} FC_CAPTURE_AND_RETHROW( (id)(record) ) }

void property_db_interface::remove( const property_id_type id )const
{ try {
    const oproperty_record prev_record = lookup( id );
    if( prev_record.valid() )
        erase_from_id_map( id );
} FC_CAPTURE_AND_RETHROW( (id) ) }

} } // bts::blockchain
