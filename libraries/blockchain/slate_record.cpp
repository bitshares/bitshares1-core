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

const slate_db_interface& slate_record::db_interface( const chain_interface& db )
{ try {
    return db._slate_db_interface;
} FC_CAPTURE_AND_RETHROW() }

oslate_record slate_db_interface::lookup( const slate_id_type id )const
{ try {
    return lookup_by_id( id );
} FC_CAPTURE_AND_RETHROW( (id) ) }

void slate_db_interface::store( const slate_id_type id, const slate_record& record )const
{ try {
    insert_into_id_map( id, record );
} FC_CAPTURE_AND_RETHROW( (id)(record) ) }

void slate_db_interface::remove( const slate_id_type id )const
{ try {
    const oslate_record prev_record = lookup( id );
    if( prev_record.valid() )
        erase_from_id_map( id );
} FC_CAPTURE_AND_RETHROW( (id) ) }

} } // bts::blockchain
