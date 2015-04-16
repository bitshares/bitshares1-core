#pragma once

#include <bts/blockchain/asset.hpp>

namespace bts { namespace blockchain {

struct status_index
{
    asset_id_type quote_id;
    asset_id_type base_id;

    friend bool operator < ( const status_index& a, const status_index& b )
    {
        return std::tie( a.quote_id, a.base_id ) < std::tie( b.quote_id, b.base_id );
    }

    friend bool operator == ( const status_index& a, const status_index& b )
    {
        return std::tie( a.quote_id, a.base_id ) == std::tie( b.quote_id, b.base_id );
    }
};

struct status_record;
typedef fc::optional<status_record> ostatus_record;

class chain_interface;
struct status_record
{
    status_index    index;
    oprice          current_feed_price;
    oprice          last_valid_feed_price;
    fc::oexception  last_error;

    status_record() {}
    status_record( const asset_id_type quote_id, const asset_id_type base_id )
        : index( status_index{ quote_id, base_id } ) {}

    void update_feed_price( const oprice& feed )
    {
        current_feed_price = feed;
        if( current_feed_price.valid() )
            last_valid_feed_price = current_feed_price;
    }

    void sanity_check( const chain_interface& )const;
    static ostatus_record lookup( const chain_interface&, const status_index );
    static void store( chain_interface&, const status_index, const status_record& );
    static void remove( chain_interface&, const status_index );

    /**************************************************************************************/
    /* All of these are no longer used but need to be kept around for applying old blocks */
    share_type               ask_depth = 0;
    share_type               bid_depth = 0;
    price                    center_price;
    price minimum_ask()const
    {
        auto avg = center_price;
        avg.ratio *= 9;
        avg.ratio /= 10;
        return avg;
    }
    price maximum_bid()const
    {
        auto avg = center_price;
        avg.ratio *= 10;
        avg.ratio /= 9;
        return avg;
    }
    /**************************************************************************************/
};

class status_db_interface
{
    friend struct status_record;

    virtual ostatus_record status_lookup_by_index( const status_index )const = 0;
    virtual void status_insert_into_index_map( const status_index, const status_record& ) = 0;
    virtual void status_erase_from_index_map( const status_index ) = 0;
};

struct string_status_record : public status_record
{
    optional<string> current_feed_price;
    optional<string> last_valid_feed_price;
};

} } // bts::blockchain

FC_REFLECT( bts::blockchain::status_index, (quote_id)(base_id) )
FC_REFLECT( bts::blockchain::status_record, (index)(current_feed_price)(last_valid_feed_price)(last_error)(ask_depth)(bid_depth)(center_price) )
FC_REFLECT_DERIVED( bts::blockchain::string_status_record, (bts::blockchain::status_record), (current_feed_price)(last_valid_feed_price) )
