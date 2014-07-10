#pragma once
#include <bts/blockchain/types.hpp>
#include <fc/io/raw.hpp>
#include <fc/io/enum_type.hpp>

namespace bts { namespace blockchain {


    struct auction_index_key
    {
        string        domain_name;
        share_type    price;
        uint32_t      bid_time;

        friend bool operator == (const auction_index_key& a, const auction_index_key& b)
        {
            return a.price == b.price;
        }
        friend bool operator < (const auction_index_key& a, const auction_index_key& b)
        {
            if (a.price < b.price)
                return true;
            if (a.price > b.price)
                return false;
            return a.bid_time < b.bid_time;
        }
    };


    struct domain_record
    {

        enum domain_state_type
        {
            unclaimed = 0,
            in_auction = 1,
            in_sale = 2,
            owned = 3
        };    

        string                                        domain_name;
        address                                       owner;
        variant                                       value;
        uint32_t                                      last_update;
        fc::enum_type<uint8_t,domain_state_type>      state;
        share_type                                    price;
        share_type                                    next_required_bid;

        uint32_t                                      time_in_top;

        auction_index_key get_auction_key() const
        {
            FC_ASSERT(this->state == in_auction, "get_auction_key:  trying to get an auction key for domain not in auction");
            auto key = auction_index_key();
            key.domain_name = this->domain_name;
            key.price = this->price;
            key.price = this->last_update;
            return key;
        }
    };

    typedef fc::optional<domain_record> odomain_record;

}}; // bts::blockchain

#include <fc/reflect/reflect.hpp>

FC_REFLECT_ENUM( bts::blockchain::domain_record::domain_state_type, (unclaimed)(in_auction)(in_sale)(owned) );
FC_REFLECT( bts::blockchain::domain_record, (domain_name)(owner)(value)(last_update)(state)(price)(next_required_bid)(time_in_top) );
FC_REFLECT( bts::blockchain::auction_index_key, (domain_name)(price)(bid_time) );
