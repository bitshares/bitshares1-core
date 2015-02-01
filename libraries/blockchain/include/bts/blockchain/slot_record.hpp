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

class chain_interface;
struct slot_db_interface;
struct slot_record
{
    slot_record(){}
    slot_record( const time_point_sec t, const account_id_type d, const optional<block_id_type>& b = optional<block_id_type>() )
    : index( d, t ), block_id( b ) {}

    slot_index              index;
    optional<block_id_type> block_id;

    static const slot_db_interface& db_interface( const chain_interface& );
};
typedef fc::optional<slot_record> oslot_record;

struct slot_db_interface
{
    std::function<oslot_record( const slot_index )>                     lookup_by_index;
    std::function<oslot_record( const time_point_sec )>                 lookup_by_timestamp;

    std::function<void( const slot_index, const slot_record& )>         insert_into_index_map;
    std::function<void( const time_point_sec, const account_id_type )>  insert_into_timestamp_map;

    std::function<void( const slot_index )>                             erase_from_index_map;
    std::function<void( const time_point_sec )>                         erase_from_timestamp_map;

    oslot_record lookup( const slot_index )const;
    oslot_record lookup( const time_point_sec )const;
    void store( const slot_index, const slot_record& )const;
    void remove( const slot_index )const;
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
