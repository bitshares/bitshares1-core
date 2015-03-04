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
    dirty_markets               = 7
};

struct property_record;
typedef fc::optional<property_record> oproperty_record;

class chain_interface;
struct property_record
{
    property_id_type    id;
    variant             value;

    void sanity_check( const chain_interface& )const;
    static oproperty_record lookup( const chain_interface&, const property_id_type );
    static void store( chain_interface&, const property_id_type, const property_record& );
    static void remove( chain_interface&, const property_id_type );
};

class property_db_interface
{
    friend struct property_record;

    virtual oproperty_record property_lookup_by_id( const property_id_type )const = 0;
    virtual void property_insert_into_id_map( const property_id_type, const property_record& ) = 0;
    virtual void property_erase_from_id_map( const property_id_type ) = 0;
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
        (dirty_markets)
        );
FC_REFLECT( bts::blockchain::property_record,
        (id)
        (value)
        );
