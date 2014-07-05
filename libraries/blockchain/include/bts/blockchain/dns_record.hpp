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
        uint32_t                                      last_update;
        fc::enum_type<uint8_t,domain_update_type>     update_type;
        share_type                                    last_bid;
        share_type                                    next_required_bid;
    };

    typedef fc::optional<domain_record> odomain_record;
}}; // bts::blockchain

#include <fc/reflect/reflect.hpp>

FC_REFLECT_ENUM( bts::blockchain::domain_record::domain_update_type, (first_bid)(bid)(sell)(info) );
FC_REFLECT( bts::blockchain::domain_record, (domain_name)(owner)(value)(last_update)(update_type)(last_bid)(next_required_bid) );
