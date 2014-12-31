#pragma once
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/object_record.hpp>


namespace bts { namespace blockchain {


    struct sale_record 
    {
        static const obj_type type = sale_object;
        object_id_type                 item = 0;
        asset                          price;
    }

    struct sale_offer_record : object_record
    {
        static const obj_type type = sale_offer_object;
        object_id_type        sale_id = 0;
        asset                 offer;
        object_id_type        new_owner_id = 0;


        // This can act as its own index
        static sale_offer_record lower_bound_for_item(const object_id_type sale_id )
        {
            auto key = sale_offer_record();
            key.sale_id = sale_id;
            return key;
        }
        friend bool operator == (const sale_offer& a, const sale_offer& b)
        {
            return a.sale_id == b.sale_id
                && a.offer_amount == b.offer_amount
                && a.new_owner_id == b.new_owner_id;
        }
        friend bool operator < (const offer_index_key& a, const offer_index_key& b)
        {
            return std::tie(a.sale_id, a.offer, a.object_id) < std::tie(b.sale_id, b.offer, b.object_id);
        }
    }


}} //bts::blockchain

FC_REFLECT( bts::blockchain::sale_record, (item)(auction_id)(owners) )
FC_REFLECT( bts::blockchain::sale_offer_record, (sale_id)(offer_amount)(new_owner_id) )
