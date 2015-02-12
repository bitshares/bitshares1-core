#include <bts/blockchain/chain_interface.hpp>
#include <bts/blockchain/slate_record.hpp>

#include <fc/crypto/sha256.hpp>
#include <fc/io/raw.hpp>

namespace bts { namespace blockchain {

slate_id_type slate_record::id()const
{
    if( slate.empty() ) return 0;
    fc::sha256::encoder enc;
    fc::raw::pack( enc, slate );
    return enc.result()._hash[ 0 ];
}

void slate_record::sanity_check( const chain_interface& db )const
{ try {
    FC_ASSERT( !slate.empty() );
} FC_CAPTURE_AND_RETHROW( (*this) ) }

oslate_record slate_record::lookup( const chain_interface& db, const slate_id_type id )
{ try {
    return db.slate_lookup_by_id( id );
} FC_CAPTURE_AND_RETHROW( (id) ) }

void slate_record::store( chain_interface& db, const slate_id_type id, const slate_record& record )
{ try {
    db.slate_insert_into_id_map( id, record );
} FC_CAPTURE_AND_RETHROW( (id)(record) ) }

void slate_record::remove( chain_interface& db, const slate_id_type id )
{ try {
    const oslate_record prev_record = db.lookup<slate_record>( id );
    if( prev_record.valid() )
        db.slate_erase_from_id_map( id );
} FC_CAPTURE_AND_RETHROW( (id) ) }

} } // bts::blockchain
