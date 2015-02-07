#pragma once

#include <bts/blockchain/types.hpp>

namespace bts { namespace blockchain {

enum class property_id_type : uint8_t
{
    database_version            = 0,
    chain_id                    = 1,
    last_asset_id               = 2,
    last_account_id             = 3,
    active_delegate_list_id     = 4,
    last_random_seed_id         = 5,
    statistics_enabled          = 6,
    /**
    *  N = num delegates
    *  Initial condition = 2N
    *  Every time a block is produced subtract 1
    *  Every time a block is missed add 2
    *  Maximum value is 2N, Min value is 0
    *
    *  Defines how many blocks you must wait to
    *  be 'confirmed' assuming that at least
    *  60% of the blocks in the last 2 rounds
    *  are present. Less than 60% and you
    *  are on the minority chain.
    */
    confirmation_requirement    = 7,
    dirty_markets               = 8,
    last_object_id              = 9
};

class chain_interface;
struct property_db_interface;
struct property_record
{
    property_id_type    id;
    variant             value;

    static const property_db_interface& db_interface( const chain_interface& );
    void sanity_check( const chain_interface& )const;
};
typedef fc::optional<property_record> oproperty_record;

struct property_db_interface
{
    std::function<oproperty_record( const property_id_type )>               lookup_by_id;
    std::function<void( const property_id_type, const property_record& )>   insert_into_id_map;
    std::function<void( const property_id_type )>                           erase_from_id_map;

    oproperty_record lookup( const property_id_type )const;
    void store( const property_id_type, const property_record& )const;
    void remove( const property_id_type )const;
};

} } // bts::blockchain

FC_REFLECT_TYPENAME( bts::blockchain::property_id_type )
FC_REFLECT_ENUM( bts::blockchain::property_id_type,
        (database_version)
        (chain_id)
        (last_asset_id)
        (last_account_id)
        (active_delegate_list_id)
        (last_random_seed_id)
        (statistics_enabled)
        (confirmation_requirement)
        (dirty_markets)
        (last_object_id)
        );
FC_REFLECT( bts::blockchain::property_record,
        (id)
        (value)
        );
