#pragma once
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/object_record.hpp>


namespace bts { namespace blockchain {


    struct site_record : object_record
    {
        static const obj_type type = site_object;
        string                 site_name;
        time_point_sec         expiration;
        asset                  renewal_fee; // monthly
        // base object_record has owner_object
        

        site_record() { site_record(""); }
        site_record( const string& name, const object_id_type auction_id = 0)
            :site_name(name),expiration(0),renewal_fee(asset())
        {}
    };

    typedef optional<site_record> osite_record;

}} //bts::blockchain

FC_REFLECT( bts::blockchain::site_record, (site_name)(expiration)(renewal_fee) )
