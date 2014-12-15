#pragma once
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/object_record.hpp>
#include <bts/blockchain/asset.hpp>
#include <fc/time.hpp>


namespace bts { namespace blockchain {


    struct site_record 
    {
        static const obj_type type;

        string                 site_name;
        time_point_sec         expiration;
        asset                  renewal_fee; // monthly
        // base object_record has owner_object
        

        site_record() { site_record(""); }
        site_record( const string& name )
        :site_name(name),expiration(0),renewal_fee(asset())
        {}
    };

    typedef optional<site_record> osite_record;

}} //bts::blockchain

FC_REFLECT( bts::blockchain::site_record, (site_name)(expiration)(renewal_fee) )
