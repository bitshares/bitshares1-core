#pragma once
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/object_record.hpp>


namespace bts { namespace blockchain {


    struct site_record : object_record
    {
        static const obj_type type = site_object;
        string                 site_name;
        object_id_type         auction_id;


        time_point_sec         expiration;
        asset                  renewal_fee; // monthly

        multisig_condition     owners;
    }

}} //bts::blockchain

FC_REFLECT( bts::blockchain::site_record, (site_name)(auction_id)(expiration)(renewal_fee)(owners) )
