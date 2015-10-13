#pragma once

#include <bts/blockchain/graphene.hpp>
#include <bts/blockchain/types.hpp>
#include <fc/time.hpp>

namespace bts { namespace blockchain {

struct snapshot_summary
{
    uint64_t num_balance_owners = 0;
    uint64_t num_asset_balances = 0;
    uint64_t num_vesting_balances = 0;

    uint64_t num_canceled_asks = 0;
    uint64_t num_canceled_bids = 0;
    uint64_t num_canceled_shorts = 0;

    uint64_t num_account_names = 0;
    uint64_t num_witnesses = 0;
    uint64_t num_workers = 0;

    uint64_t num_mias = 0;
    uint64_t num_collateral_positions = 0;
    uint64_t num_uias = 0;
};

struct snapshot_vesting_balance
{
    time_point_sec  start_time;
    uint32_t        duration_seconds = 0;
    share_type      original_balance = 0;
    share_type      current_balance = 0;
};

struct snapshot_balance
{
    map<string, share_type>             assets;
    optional<snapshot_vesting_balance>  vesting;
};

struct snapshot_account
{
    uint32_t id;
    time_point_sec timestamp;
    public_key_type owner_key;
    public_key_type active_key;
    optional<public_key_type> signing_key;
    optional<share_type> daily_pay;
};

struct snapshot_debt
{
    share_type collateral = 0;
    share_type debt = 0;
};

struct snapshot_asset
{
    uint32_t id;
    string      owner;

    string      description;
    uint64_t    precision = 1;

    share_type  max_supply = 0;
    share_type  collected_fees = 0;

    map<address, snapshot_debt> debts;
};

struct snapshot_state
{
    signed_block_header             head_block;
    share_type                      max_core_supply = 0;
    snapshot_summary                summary;
    map<address, snapshot_balance>  balances;
    map<string, snapshot_account>   accounts;
    map<string, snapshot_asset>     assets;
};

} } // bts::blockchain

FC_REFLECT( bts::blockchain::snapshot_summary, (num_balance_owners)(num_asset_balances)(num_vesting_balances)
                                               (num_canceled_asks)(num_canceled_bids)(num_canceled_shorts)
                                               (num_account_names)(num_witnesses)(num_workers)
                                               (num_mias)(num_collateral_positions)(num_uias))
FC_REFLECT( bts::blockchain::snapshot_vesting_balance, (start_time)(duration_seconds)(original_balance)(current_balance) )
FC_REFLECT( bts::blockchain::snapshot_balance, (assets)(vesting) )
FC_REFLECT( bts::blockchain::snapshot_account, (id)(timestamp)(owner_key)(active_key)(signing_key)(daily_pay) )
FC_REFLECT( bts::blockchain::snapshot_debt, (collateral)(debt) )
FC_REFLECT( bts::blockchain::snapshot_asset, (id)(owner)(description)(precision)(max_supply)(collected_fees)(debts) )
FC_REFLECT( bts::blockchain::snapshot_state, (head_block)(max_core_supply)(summary)(balances)(accounts)(assets) )
