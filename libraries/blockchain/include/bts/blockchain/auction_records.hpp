// Auction the rights to an object for the benefit of the current owner or for the DAC

#pragma once
#include <bts/blockchain/asset.hpp>
#include <bts/blockchain/object_record.hpp>
#include <bts/blockchain/types.hpp>
#include <fc/time.hpp>

namespace bts { namespace blockchain {

    class chain_interface;

    struct user_auction_record : object_record
    {
        static const obj_type type = user_auction_object;

        object_id_type                   item;
        object_id_type                   original_owner;
        object_id_type                   beneficiary;
        time_point_sec                   expiration;
        asset                            buy_now;
        int                              user_fee; // fee or kickback as part per million
        
        object_id_type                   previous_bidder;
        asset                            previous_bid;
        time_point_sec                   previous_bid_time;
        int                              bid_count;
        asset                            bid_total;
        asset                            total_fees;

        bool                             object_claimed;  // The auction winner can claim
        bool                             balance_claimed;

        bool                             is_complete( const chain_interface& chain );
    };

    // This is currently only created by site operations, not by auction_start!
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

        bool                             is_complete( const chain_interface& chain );
        object_id_type                   original_winner(); // what the owner of item was when this was won


        throttled_auction_record( const object_id_type item );
    };


    struct throttled_auction_index_key
    {
        object_id_type        item;
        asset                 bid;
        time_point_sec        time;

        friend bool operator == (const throttled_auction_index_key& a, const throttled_auction_index_key& b)
        {
            return std::tie(a.item, a.bid) == std::tie(b.item, b.bid);
        }
        friend bool operator < (const throttled_auction_index_key& a, const throttled_auction_index_key& b)
        {
            return std::tie(a.bid, b.time, b.item) > std::tie(b.bid, a.time, a.item);
        }
    };


}} //bts::blockchain

FC_REFLECT( bts::blockchain::user_auction_record,
        (item)
        (original_owner)
        (beneficiary)
        (expiration)
        (buy_now)
        (user_fee)
        (previous_bidder)
        (previous_bid)
        (previous_bid_time)
        (bid_count)
        (bid_total)
        (total_fees)
        (object_claimed)
        (balance_claimed)
        )
FC_REFLECT( bts::blockchain::throttled_auction_record,
        (item)
        (bidder)
        (bid)
        (bid_time)
        (prior_bid)
        (time_in_spotlight)
        (fee)
        )
FC_REFLECT( bts::blockchain::throttled_auction_index_key, (item)(bid)(time) )
