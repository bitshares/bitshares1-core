#pragma once
#include <bts/blockchain/types.hpp>
#include <fc/io/raw.hpp>
#include <fc/io/enum_type.hpp>

namespace bts { namespace blockchain {

    struct domain_record
    {

        enum domain_update_type
        {
            first_bid = 0,
            bid = 1,
            sell = 2,
            info = 3
        };    

        string                                        domain_name;
        address                                       owner;
        variant                                       value;
        uint32_t                                      first_bid_time;
        uint32_t                                      last_update;
        fc::enum_type<uint8_t,domain_update_type>     update_type;
        share_type                                    last_bid;
        share_type                                    next_required_bid;

        uint32_t                                      time_in_top;
    };

    typedef fc::optional<domain_record> odomain_record;


    struct auction_index_key
    {
        string        domain_name;
        share_type    last_bid;
        uint32_t      first_bid_time;

        friend bool operator == (const auction_index_key& a, const auction_index_key& b)
        {
            return a.last_bid == b.last_bid;
        }
        friend bool operator < (const auction_index_key& a, const auction_index_key& b)
        {
            if (a.last_bid < b.last_bid)
                return true;
            if (a.last_bid > b.last_bid)
                return false;
            return a.first_bid_time < b.first_bid_time;
        }
    };

}}; // bts::blockchain

#include <fc/reflect/reflect.hpp>

FC_REFLECT_ENUM( bts::blockchain::domain_record::domain_update_type, (first_bid)(bid)(sell)(info) );
FC_REFLECT( bts::blockchain::domain_record, (domain_name)(owner)(value)(last_update)(update_type)(last_bid)(next_required_bid)(time_in_top) );
FC_REFLECT( bts::blockchain::auction_index_key, (domain_name)(last_bid)(first_bid_time) );
