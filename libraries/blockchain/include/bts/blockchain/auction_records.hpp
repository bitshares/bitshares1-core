// Auction the rights to an object for the benefit of the current owner or for the DAC

#pragma once
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/object_record.hpp>


namespace bts { namespace blockchain {

    struct user_auction_record : object_record
    {
        static const obj_type type = throttled_auction_object;

        object_id_type                   item;
        time_point_sec                   expiration;
        asset                            buy_now;
        int                              user_fee; // fee or kickback as part per million
        
        object_id_type                   previous_bidder;
        asset                            previous_bid;
        time_point_sec                   previous_bid_time;
        int                              bid_count;
        asset                            bid_total;
        asset                            total_fees;

        bool                             is_complete();
        object_id_type                   original_winner(); // what the owner of item was when this auction was won
    }


    struct throttled_auction_record : object_record
    {
        static const obj_type type = throttled_auction_object;

        object_id_type                   item;
        object_id_type                   bidder;
        asset                            bid;
        time_point_sec                   bid_time;
        asset                            prior_bid;

        time_point_sec                   time_in_spotlight;
        int                              fee; // fee or kickback as part per million

        bool                             is_complete();
        object_id_type                   original_winner(); // what the owner of item was when this was won
    }


    struct throttled_auction_index_key
    {
        object_id_type        item;
        asset                 bid;
        time_point_sec        time;

        friend bool operator == (const auction_index_key& a, const auction_index_key& b)
        {
            return std::tie(a.item, a.price) == std::tie(b.item, b.price);
        }
        friend bool operator < (const auction_index_key& a, const auction_index_key& b)
        {
            return std::tie(a.price, b.bid_time, b.item) > std::tie(b.price, a.bid_time, a.item);
        }
    };


}} //bts::blockchain
