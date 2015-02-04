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

    class chain_interface;
    struct feed_db_interface;
    struct feed_record
    {
        feed_index      index;
        price           value;
        time_point_sec  last_update;

        static const feed_db_interface& db_interface( const chain_interface& );
    };
    typedef fc::optional<feed_record> ofeed_record;

    struct feed_db_interface
    {
        std::function<ofeed_record( const feed_index )>             lookup_by_index;
        std::function<void( const feed_index, const feed_record& )> insert_into_index_map;
        std::function<void( const feed_index )>                     erase_from_index_map;

        ofeed_record lookup( const feed_index )const;
        void store( const feed_index, const feed_record& )const;
        void remove( const feed_index )const;
    };

    struct feed_entry
    {
        string                         delegate_name;
        double                         price;
        time_point_sec                 last_update;
        fc::optional<string>           asset_symbol;
        fc::optional<double>           median_price;
    };

} } // bts::blockchain

FC_REFLECT( bts::blockchain::feed_index, (quote_id)(delegate_id) )
FC_REFLECT( bts::blockchain::feed_record, (index)(value)(last_update) )
FC_REFLECT( bts::blockchain::feed_entry, (delegate_name)(price)(last_update)(asset_symbol)(median_price) );
