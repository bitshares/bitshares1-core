/* 
 * This file contains structs to populate for JSON representation of transactions/operations
 * for wallets and block explorers and such 
 */

#pragma once

#include <bts/blockchain/address.hpp>
#include <bts/blockchain/extended_address.hpp>
#include <fc/variant.hpp>

namespace bts { namespace wallet {

using namespace bts::blockchain;

struct public_key_summary
{
    string    hex;
    string    native_pubkey;
    string    native_address;
    string    pts_normal_address;
    string    pts_compressed_address;
    string    btc_normal_address;
    string    btc_compressed_address;
};

struct pretty_ledger_entry
{
   string   from_account;
   string   to_account;
   asset    amount;
   string   memo;
};

struct pretty_transaction
{
    pretty_transaction()
    :is_virtual(false),is_confirmed(false),is_market(false),is_market_cancel(false),block_num(0){}

    bool                                        is_virtual;
    bool                                        is_confirmed;
    bool                                        is_market;
    bool                                        is_market_cancel;
    transaction_id_type                         trx_id;
    uint32_t                                    block_num;
    vector<pretty_ledger_entry>                 ledger_entries;
    asset                                       fee;
    fc::time_point_sec                          created_time;
    fc::time_point_sec                          received_time;
    map<asset_id_type, asset>      running_balances;
    //map<string, map<asset_id_type, asset>>      running_balances;

    template<typename T>
    void add_operation( const T& op ) { operations.push_back( fc::variant(op) ); }

    std::vector<fc::variant>                    operations;
};

struct pretty_null_op
{
    pretty_null_op():op_name("nullop"){}
    std::string                                 op_name;
};

struct pretty_withdraw_op 
{
    pretty_withdraw_op():op_name("withdraw"){}
    std::string                                 op_name;
    std::pair<address, std::string>             owner;
    share_type                                  amount;
    //std::pair<account_id_type, std::string>   vote;   TODO how to get this?
};

struct pretty_deposit_op 
{
    pretty_deposit_op():op_name("deposit"){}
    std::string                                 op_name;
    std::pair<address, std::string>             owner;
    share_type                                  amount;
    std::pair<account_id_type, std::string>     vote;
};

struct pretty_reserve_name_op
{
    pretty_reserve_name_op():op_name("reserve_name"){}
    std::string                                 op_name;
    std::string                                 name;
    fc::variant                                 json_data;
    public_key_type                             owner_key;
    extended_public_key                         active_key;
    bool                                        is_delegate;
};

struct pretty_update_name_op
{
    pretty_update_name_op():op_name("update_name"),WARNING("unimplemented pretty op"){}
    std::string                                 op_name;
    std::string                                 WARNING;
};

struct pretty_create_asset_op
{
    pretty_create_asset_op():op_name("create_asset"),WARNING("unimplemented pretty op"){}
    std::string                                 op_name;
    std::string                                 WARNING;
};

struct pretty_update_asset_op
{
    pretty_update_asset_op():op_name("update_asset"),WARNING("unimplemented pretty op"){}
    std::string                                 op_name;
    std::string                                 WARNING;
};

struct pretty_issue_asset_op
{
    pretty_issue_asset_op():op_name("issue_asset"),WARNING("unimplemented pretty op"){}
    std::string                                 op_name;
    std::string                                 WARNING;
};

struct pretty_fire_delegate_op
{
    pretty_fire_delegate_op():op_name("fire_delegate"),WARNING("unimplemented pretty op"){}
    std::string                                 op_name;
    std::string                                 WARNING;
};

struct pretty_submit_proposal_op
{
    pretty_submit_proposal_op():op_name("submit_proposal"),WARNING("unimplemented pretty op"){}
    std::string                                 op_name;
    std::string                                 WARNING;
};

struct pretty_vote_proposal_op
{
    pretty_vote_proposal_op():op_name("vote_proposal"),WARNING("unimplemented pretty op"){}
    std::string                                 op_name;
    std::string                                 WARNING;
};

struct pretty_bid_op
{
    pretty_bid_op():op_name("bid"),WARNING("unimplemented pretty op"){}
    std::string                                 op_name;
    std::string                                 WARNING;
};

struct pretty_ask_op
{
    pretty_ask_op():op_name("ask"),WARNING("unimplemented pretty op"){}
    std::string                                 op_name;
    std::string                                 WARNING;
};

struct pretty_short_op
{
    pretty_short_op():op_name("short"),WARNING("unimplemented pretty op"){}
    std::string                                 op_name;
    std::string                                 WARNING;
};

struct pretty_cover_op
{
    pretty_cover_op():op_name("cover"),WARNING("unimplemented pretty op"){}
    std::string                                 op_name;
    std::string                                 WARNING;
};

struct pretty_add_collateral_op
{
    pretty_add_collateral_op():op_name("add_collateral"),WARNING("unimplemented pretty op"){}
    std::string                                 op_name;
    std::string                                 WARNING;
};


struct pretty_remove_collateral_op
{
    pretty_remove_collateral_op():op_name("remove_collateral"),WARNING("unimplemented pretty op"){}
    std::string                                 op_name;
    std::string                                 WARNING;
};


}} // bts::wallet

FC_REFLECT( bts::wallet::public_key_summary, (hex)(native_pubkey)(native_address)(pts_normal_address)(pts_compressed_address)(btc_normal_address)(btc_compressed_address) );
FC_REFLECT( bts::wallet::pretty_ledger_entry, (from_account)(to_account)(amount)(memo) );
FC_REFLECT( bts::wallet::pretty_transaction, (is_virtual)(is_confirmed)(is_market)(is_market_cancel)(trx_id)(block_num)(ledger_entries)(fee)(created_time)(received_time)(running_balances) );
FC_REFLECT( bts::wallet::pretty_withdraw_op, (op_name)(owner)(amount));
FC_REFLECT( bts::wallet::pretty_deposit_op, (op_name)(owner)(amount)(vote));
FC_REFLECT( bts::wallet::pretty_reserve_name_op, (op_name)(name)(json_data)(owner_key)(active_key)(is_delegate));
FC_REFLECT( bts::wallet::pretty_update_name_op, (op_name)(WARNING) );
FC_REFLECT( bts::wallet::pretty_create_asset_op, (op_name)(WARNING) );
FC_REFLECT( bts::wallet::pretty_update_asset_op, (op_name)(WARNING) );
FC_REFLECT( bts::wallet::pretty_issue_asset_op, (op_name)(WARNING) );
FC_REFLECT( bts::wallet::pretty_fire_delegate_op, (op_name)(WARNING) );
FC_REFLECT( bts::wallet::pretty_submit_proposal_op, (op_name)(WARNING) );
FC_REFLECT( bts::wallet::pretty_vote_proposal_op, (op_name)(WARNING) );
FC_REFLECT( bts::wallet::pretty_bid_op, (op_name)(WARNING) );
FC_REFLECT( bts::wallet::pretty_ask_op, (op_name)(WARNING) );
FC_REFLECT( bts::wallet::pretty_short_op, (op_name)(WARNING) );
FC_REFLECT( bts::wallet::pretty_cover_op, (op_name)(WARNING) );
FC_REFLECT( bts::wallet::pretty_add_collateral_op, (op_name)(WARNING) );
FC_REFLECT( bts::wallet::pretty_remove_collateral_op, (op_name)(WARNING) );
