#pragma once
#include <bts/blockchain/operations.hpp>
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/dns_record.hpp>
#include <fc/io/enum_type.hpp>

namespace bts { namespace blockchain {

    struct domain_bid_operation
    {
        static const operation_type_enum type;
        void evaluate( transaction_evaluation_state& eval_state );

        string         domain_name;
        address        bidder_address;
        share_type     bid_amount;
    };

    struct domain_buy_operation
    {
        static const operation_type_enum type;
        void evaluate( transaction_evaluation_state& eval_state );
    };

    struct domain_sell_operation
    {
        static const operation_type_enum type;
        void evaluate( transaction_evaluation_state& eval_state );
    };

    struct domain_cancel_sell_operation
    {
        static const operation_type_enum type;
        void evaluate( transaction_evaluation_state& eval_state );
    };

    struct domain_update_info_operation
    {
        static const operation_type_enum type;
        void evaluate( transaction_evaluation_state& eval_state );
    };




/*
    struct update_domain_operation
    {
        update_domain_operation():domain_name(""){}

        static const operation_type_enum type;

        address                                                         owner;
        string                                                          domain_name;
        variant                                                         value;
        fc::enum_type<uint8_t, domain_record::domain_state_type>        update_type;
        share_type                                                      bid_amount;

        void evaluate( transaction_evaluation_state& eval_state );
    };
*/

}}; // bts::blockchain

FC_REFLECT( bts::blockchain::domain_bid_operation, (domain_name)(bidder_address)(bid_amount) );
