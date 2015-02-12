#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/property_record.hpp>

namespace bts { namespace blockchain {

void property_record::sanity_check( const chain_interface& db )const
{ try {
    FC_ASSERT( !value.is_null() );
} FC_CAPTURE_AND_RETHROW( (*this) ) }

oproperty_record property_record::lookup( const chain_interface& db, const property_id_type id )
{ try {
    return db.property_lookup_by_id( id );
} FC_CAPTURE_AND_RETHROW( (id) ) }

void property_record::store( chain_interface& db, const property_id_type id, const property_record& record )
{ try {
    db.property_insert_into_id_map( id, record );
} FC_CAPTURE_AND_RETHROW( (id)(record) ) }

void property_record::remove( chain_interface& db, const property_id_type id )
{ try {
    const oproperty_record prev_record = db.lookup<property_record>( id );
    if( prev_record.valid() )
        db.property_erase_from_id_map( id );
} FC_CAPTURE_AND_RETHROW( (id) ) }

} } // bts::blockchain
