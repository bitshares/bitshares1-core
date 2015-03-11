#pragma once

#include <bts/blockchain/asset.hpp>
#include <fc/time.hpp>

namespace bts { namespace blockchain {

struct feed_index
{
    asset_id_type   quote_id;
    account_id_type delegate_id;

    friend bool operator < ( const feed_index& a, const feed_index& b )
    {
        return std::tie( a.quote_id, a.delegate_id ) < std::tie( b.quote_id, b.delegate_id );
    }

    friend bool operator == ( const feed_index& a, const feed_index& b )
    {
        return std::tie( a.quote_id, a.delegate_id ) == std::tie( b.quote_id, b.delegate_id );
    }
};

struct feed_record;
typedef fc::optional<feed_record> ofeed_record;

class chain_interface;
struct feed_record
{
    feed_index      index;
    price           value;
    time_point_sec  last_update;

    void sanity_check( const chain_interface& )const;
    static ofeed_record lookup( const chain_interface&, const feed_index );
    static void store( chain_interface&, const feed_index, const feed_record& );
    static void remove( chain_interface&, const feed_index );
};

class feed_db_interface
{
    friend struct feed_record;

    virtual ofeed_record feed_lookup_by_index( const feed_index )const = 0;
    virtual void feed_insert_into_index_map( const feed_index, const feed_record& ) = 0;
    virtual void feed_erase_from_index_map( const feed_index ) = 0;
};

struct feed_entry
{
    string                         delegate_name;
    string                         price;
    time_point_sec                 last_update;
    fc::optional<string>           asset_symbol;
    fc::optional<string>           median_price;
};

} } // bts::blockchain

FC_REFLECT( bts::blockchain::feed_index, (quote_id)(delegate_id) )
FC_REFLECT( bts::blockchain::feed_record, (index)(value)(last_update) )
FC_REFLECT( bts::blockchain::feed_entry, (delegate_name)(price)(last_update)(asset_symbol)(median_price) );
