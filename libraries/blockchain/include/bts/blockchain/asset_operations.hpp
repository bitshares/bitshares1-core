#pragma once

#include <bts/blockchain/asset.hpp>
#include <bts/blockchain/operations.hpp>
#include <bts/blockchain/account_record.hpp>
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/asset_record.hpp>

namespace bts { namespace blockchain {

bool is_power_of_ten( uint64_t n );

struct create_asset_operation
{
    static const operation_type_enum type;

    string          symbol;
    string          name;
    string          description;
    variant         public_data;

    account_id_type issuer_account_id;

    share_type      maximum_share_supply = 0;
    uint64_t        precision = 0;

    void evaluate( transaction_evaluation_state& eval_state )const;
};

// TODO: Rename because it can now withdraw fees in addition to issue shares
struct issue_asset_operation
{
    static const operation_type_enum type;

    asset amount;

    void evaluate( transaction_evaluation_state& eval_state )const;
};

struct asset_update_properties_operation
{
    static const operation_type_enum type;

    asset_id_type               asset_id;

    optional<account_id_type>   issuer_id;

    optional<string>            name;
    optional<string>            description;
    optional<variant>           public_data;

    optional<uint64_t>          precision;
    optional<share_type>        max_supply;

    optional<share_type>        withdrawal_fee;
    optional<uint16_t>          market_fee_rate;

    void evaluate( transaction_evaluation_state& eval_state )const;
};

struct asset_update_permissions_operation
{
    static const operation_type_enum type;

    asset_id_type                   asset_id;

    optional<multisig_meta_info>    authority;
    optional<uint32_t>              authority_flag_permissions;
    optional<uint32_t>              active_flags;

    void evaluate( transaction_evaluation_state& eval_state )const;
};

struct asset_update_whitelist_operation
{
    static const operation_type_enum type;

    asset_id_type           asset_id;
    set<account_id_type>    account_ids;

    void evaluate( transaction_evaluation_state& eval_state )const;
};

} } // bts::blockchain

FC_REFLECT( bts::blockchain::create_asset_operation,
        (symbol)
        (name)
        (description)
        (public_data)
        (issuer_account_id)
        (maximum_share_supply)
        (precision)
        )
FC_REFLECT( bts::blockchain::issue_asset_operation,
        (amount)
        )
FC_REFLECT( bts::blockchain::asset_update_properties_operation,
        (asset_id)
        (issuer_id)
        (name)
        (description)
        (public_data)
        (precision)
        (max_supply)
        (withdrawal_fee)
        (market_fee_rate)
        )
FC_REFLECT( bts::blockchain::asset_update_permissions_operation,
        (asset_id)
        (authority)
        (authority_flag_permissions)
        (active_flags)
        )
FC_REFLECT( bts::blockchain::asset_update_whitelist_operation,
        (asset_id)
        (account_ids)
        )
