/* 
 * This file contains structs to populate for JSON representation of transactions/operations
 * for wallets and block explorers and such 
 */

#pragma once

#include <bts/blockchain/address.hpp>
#include <fc/variant.hpp>

namespace bts { namespace wallet {

using namespace bts::blockchain;

struct pretty_transaction
{
    uint32_t                                    number;
    uint32_t                                    block_num;
    uint32_t                                    tx_num;
    transaction_id_type                         tx_id;
    uint32_t                                    timestamp;
    std::map<std::string, share_type>           totals_in;
    std::map<std::string, share_type>           totals_out;
    std::map<std::string, share_type>           fees;

    template<typename T>
    void add_operation( const T& op ) { operations.push_back( fc::variant(op) ); }

    std::vector<fc::variant>                    operations;
};

struct pretty_withdraw_op 
{
    pretty_withdraw_op():op_name("withdraw"){}
    std::string                                 op_name;
    std::pair<address, std::string>             owner;
    share_type                                  amount;
    //std::pair<name_id_type, std::string>        vote;   TODO how to get this?
};

struct pretty_deposit_op 
{
    pretty_deposit_op():op_name("deposit"){}
    std::string                                 op_name;
    std::pair<address, std::string>             owner;
    share_type                                  amount;
    std::pair<name_id_type, std::string>        vote;
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

}} // bts::wallet

FC_REFLECT( bts::wallet::pretty_transaction, (number)(block_num)(tx_num)(timestamp)(fees)(tx_id) (operations)(totals_in)(totals_out)(fees));
FC_REFLECT( bts::wallet::pretty_withdraw_op, (op_name)(owner)(amount));
FC_REFLECT( bts::wallet::pretty_deposit_op, (op_name)(owner)(amount)(vote));
FC_REFLECT( bts::wallet::pretty_reserve_name_op, (op_name)(name)(json_data)(owner_key)(active_key)(is_delegate));
