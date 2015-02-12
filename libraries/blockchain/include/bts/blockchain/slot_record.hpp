#pragma once
#include <bts/blockchain/types.hpp>
#include <fc/time.hpp>

namespace bts { namespace blockchain {

struct slot_index
{
    account_id_type delegate_id;
    time_point_sec  timestamp;

    slot_index() {}
    slot_index( const account_id_type id, const time_point_sec t )
    : delegate_id( id ), timestamp( t ) {}

    friend bool operator < ( const slot_index& a, const slot_index& b )
    {
        // Reverse because newer slot records are what people care about
        return std::tie( a.delegate_id, a.timestamp ) > std::tie( b.delegate_id, b.timestamp );
    }

    friend bool operator == ( const slot_index& a, const slot_index& b )
    {
        return std::tie( a.delegate_id, a.timestamp ) == std::tie( b.delegate_id, b.timestamp );
    }
};

struct slot_record;
typedef fc::optional<slot_record> oslot_record;

class chain_interface;
struct slot_record
{
    slot_record(){}
    slot_record( const time_point_sec t, const account_id_type d, const optional<block_id_type>& b = optional<block_id_type>() )
    : index( d, t ), block_id( b ) {}

    slot_index              index;
    optional<block_id_type> block_id;

    void sanity_check( const chain_interface& )const;
    static oslot_record lookup( const chain_interface&, const slot_index );
    static oslot_record lookup( const chain_interface&, const time_point_sec );
    static void store( chain_interface&, const slot_index, const slot_record& );
    static void remove( chain_interface&, const slot_index );
};

class slot_db_interface
{
    friend struct slot_record;

    virtual oslot_record slot_lookup_by_index( const slot_index )const = 0;
    virtual oslot_record slot_lookup_by_timestamp( const time_point_sec )const = 0;

    virtual void slot_insert_into_index_map( const slot_index, const slot_record& ) = 0;
    virtual void slot_insert_into_timestamp_map( const time_point_sec, const account_id_type ) = 0;

    virtual void slot_erase_from_index_map( const slot_index ) = 0;
    virtual void slot_erase_from_timestamp_map( const time_point_sec ) = 0;
};

} } // bts::blockchain

FC_REFLECT( bts::blockchain::slot_index,
            (delegate_id)
            (timestamp)
            )

FC_REFLECT( bts::blockchain::slot_record,
            (index)
            (block_id)
            )
