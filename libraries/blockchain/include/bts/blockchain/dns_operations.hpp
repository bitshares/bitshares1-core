#pragma once
#include <bts/blockchain/operations.hpp>
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/withdraw_types.hpp> // titan
#include <bts/blockchain/dns_record.hpp>
#include <fc/io/enum_type.hpp>
#include <fc/io/raw.hpp>

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

        string   domain_name;
        address  new_owner;
    };

    struct domain_sell_operation
    {
        static const operation_type_enum type;
        void evaluate( transaction_evaluation_state& eval_state );
    
        string       domain_name;
        share_type   price;
    };

    struct domain_cancel_sell_operation
    {
        static const operation_type_enum type;
        void evaluate( transaction_evaluation_state& eval_state );

        string domain_name;
    };

    struct domain_transfer_operation
    {
        static const operation_type_enum type;
        void evaluate( transaction_evaluation_state& eval_state );

        void titan_transfer( const fc::ecc::private_key& one_time_private_key,
                             const fc::ecc::public_key& to_public_key,
                             const fc::ecc::private_key& from_private_key,
                             const std::string& memo_message,
                             const fc::ecc::public_key& memo_pub_key,
                             memo_flags_enum memo_type = from_memo );
        // TODO all titan crypto ops should be factored out of here
        omemo_status    decrypt_memo_data( const fc::ecc::private_key& receiver_key ) const;
        memo_data    decrypt_memo_data( const fc::sha512& secret ) const;

        string                      domain_name;
        address                     owner;
        fc::optional<titan_memo>    memo;

    };


    struct domain_update_info_operation
    {
        static const operation_type_enum type;
        void evaluate( transaction_evaluation_state& eval_state );

        string domain_name;
        variant value;

    };


}}; // bts::blockchain

FC_REFLECT( bts::blockchain::domain_bid_operation, (domain_name)(bidder_address)(bid_amount) );
FC_REFLECT( bts::blockchain::domain_update_info_operation, (domain_name)(value) );
FC_REFLECT( bts::blockchain::domain_sell_operation, (domain_name)(price) );
FC_REFLECT( bts::blockchain::domain_cancel_sell_operation, (domain_name) );
FC_REFLECT( bts::blockchain::domain_buy_operation, (domain_name)(new_owner) );
FC_REFLECT( bts::blockchain::domain_transfer_operation, (domain_name)(owner)(memo) );
