#pragma once
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/dns_config.hpp>
#include <fc/io/raw.hpp>
#include <fc/io/enum_type.hpp>

#include <fc/time.hpp>

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
            // most expensive first
            if (a.price > b.price)
                return true;
            if (a.price < b.price)
                return false;
            return a.bid_time < b.bid_time;
        }
    };

    struct offer_index_key
    {
        string        domain_name; 
        share_type    price;
        address       offer_address;
        uint32_t      offer_time;

        offer_index_key() {};

        offer_index_key(const string& domain_name, const share_type& price)
        :domain_name(domain_name),price(price) {};

        static offer_index_key lower_bound_for_domain(const string& domain)
        {
            auto key = offer_index_key();
            key.domain_name = domain;
            key.price = BTS_BLOCKCHAIN_MAX_SHARES;
            return key;
        }

        friend bool operator == (const offer_index_key& a, const offer_index_key& b)
        {
            return a.domain_name == b.domain_name 
                && a.offer_address == b.offer_address
                && a.price == b.price;
        }
        friend bool operator < (const offer_index_key& a, const offer_index_key& b)
        {
            if (a.domain_name < b.domain_name)
                return true;
            if (a.domain_name > b.domain_name)
                return false;
            // most expensive first
            if (a.price > b.price)
                return true;
            if (a.price < b.price)
                return false;
            if (a.offer_time < b.offer_time)
                return true;
            if (a.offer_time > b.offer_time)
                return false;

            return a.offer_address < b.offer_address;
        }

    };

    typedef fc::optional<offer_index_key>           ooffer_index_key;

    struct domain_record
    {
        domain_record():domain_name(""),value(variant("")),last_update(0),time_in_top(0){};
        enum domain_state_type
        {
            unclaimed,
            in_auction,
            in_sale,
            owned
        };    

        string                                        domain_name;
        address                                       owner;
        variant                                       value;
        uint32_t                                      last_update;
        fc::enum_type<uint8_t,domain_state_type>      state;
        share_type                                    price;
        share_type                                    next_required_bid;

        uint32_t                                      time_in_top;

        domain_state_type get_true_state() const
        {
            if (domain_state_type(this->state) == owned)
            {
                if (fc::time_point::now().sec_since_epoch() > last_update + P2P_EXPIRE_DURATION_SECS)
                    return unclaimed;
                return owned;
            }
            return this->state;
        }

        auction_index_key get_auction_key() const
        {
            FC_ASSERT(this->state == in_auction, "get_auction_key:  trying to get an auction key for domain not in auction");
            auto key = auction_index_key();
            key.domain_name = this->domain_name;
            key.price = this->price;
            key.bid_time = this->last_update;
            return key;
        }
    };


    typedef fc::optional<domain_record>           odomain_record;

}}; // bts::blockchain

#include <fc/reflect/reflect.hpp>

FC_REFLECT_ENUM( bts::blockchain::domain_record::domain_state_type, (unclaimed)(in_auction)(in_sale)(owned) );
FC_REFLECT( bts::blockchain::domain_record, (domain_name)(owner)(value)(last_update)(state)(price)(next_required_bid)(time_in_top) );
FC_REFLECT( bts::blockchain::auction_index_key, (domain_name)(price)(bid_time) );
FC_REFLECT( bts::blockchain::offer_index_key, (domain_name)(price)(offer_address)(offer_time) );
