#pragma once
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/asset.hpp>
#include <bts/blockchain/operations.hpp>
#include <bts/blockchain/condition.hpp>

namespace bts { namespace blockchain {

    // This creates a site owned by the DAC and a special auction with an automatic bid
    struct site_create_operation
    {
        static const operation_type_enum type;

        string                           site_name;
        object_id_type                   new_owner;

        void evaluate( transaction_evaluation_state& eval_state )const;
    };

    // This should be doable with set_object as only user data and owners are changed
    struct site_update_operation
    {
        static const operation_type_enum type;

        object_id_type                   site_id;
        object_id_type                   owner_id;
        variant                          user_data;
        asset                            lease_payment;

        void evaluate( transaction_evaluation_state& eval_state )const;
    };


}} // bts::blockchain

