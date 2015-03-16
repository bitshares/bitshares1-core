#pragma once

#include <bts/blockchain/asset.hpp>
#include <bts/blockchain/operations.hpp>
#include <bts/blockchain/withdraw_types.hpp>

namespace bts { namespace blockchain {

/** withdraws funds and moves them into the transaction
* balance making them available for deposit
*/
struct withdraw_operation
{
    static const operation_type_enum type;

    withdraw_operation():amount(0){}

    withdraw_operation( const balance_id_type& id, share_type amount_arg )
        :balance_id(id),amount(amount_arg){ FC_ASSERT( amount_arg > 0 ); }

    /** the account to withdraw from */
    balance_id_type    balance_id;
    /** that amount to withdraw from the account*/
    share_type         amount;
    /** any data required by the claim_condition */
    std::vector<char>  claim_input_data;

    void evaluate( transaction_evaluation_state& eval_state )const;
    void evaluate_v3( transaction_evaluation_state& eval_state )const;
    void evaluate_v2( transaction_evaluation_state& eval_state )const;
    void evaluate_v1( transaction_evaluation_state& eval_state )const;
};

/**
*  The first time a deposit is made to a new address
*  the condition under which it may be spent must be
*  defined.  After the first deposit then future
*  deposits merely reference the address.
*/
struct deposit_operation
{
    static const operation_type_enum type;
    /** owner is just the hash of the condition */
    balance_id_type                balance_id()const;

    deposit_operation():amount(0){}
    deposit_operation( const address& owner, const asset& amnt, slate_id_type slate_id = 0 );

    /** the condition that the funds may be withdrawn,
    *  this is only necessary if the address is new.
    */
    share_type                       amount;
    withdraw_condition               condition;

    void evaluate( transaction_evaluation_state& eval_state )const;
    void evaluate_v2( transaction_evaluation_state& eval_state )const;
    void evaluate_v1( transaction_evaluation_state& eval_state )const;
};

/**
*  Burn operations takes shares out of circulation unless they
*  are BitAssets in which case it goes to collected fees and is
*  distributed as yield.
*/
struct burn_operation
{
    static const operation_type_enum type;

    burn_operation( asset amount_to_burn = asset(),
                    account_id_type burn_for_or_against = 0,
                    const string& public_message = "",
                    optional<signature_type> sig = optional<signature_type>() )
        :amount(amount_to_burn),account_id(burn_for_or_against),message(public_message),message_signature(sig){}

    /** the condition that the funds may be withdrawn,
    *  this is only necessary if the address is new.
    */
    asset                        amount;
    account_id_type              account_id;
    string                       message;
    optional<signature_type>     message_signature;

    void evaluate( transaction_evaluation_state& eval_state )const;
    void evaluate_v1( transaction_evaluation_state& eval_state )const;
};

struct release_escrow_operation
{
    static const operation_type_enum type;

    balance_id_type  escrow_id;
    address          released_by;
    share_type       amount_to_receiver = 0;
    share_type       amount_to_sender   = 0;

    void evaluate( transaction_evaluation_state& eval_state )const;
};

/* Moves funds to a new balance with the same owner key but different votes
* and restricted owner key.
*/
struct update_balance_vote_operation
{
    static const operation_type_enum type;

    balance_id_type     balance_id;
    optional<address>   new_restricted_owner;
    slate_id_type       new_slate;

    void evaluate( transaction_evaluation_state& eval_state )const;
};

/**
*  This operation enforces the expected fee for a
*  transaction. If the transaction evaluation shows
*  that more than the expected fee would be paid then
*  it fails.
*
*  The purpose of this is to handle cases where the
*  fee is not entirely deterministic from the withdraws
*  and deposits. This happens if there is interest,
*  yield, or market operations that effect the
*  result. It is also a way for the creator of a
*  transaction to protect against malformed transactions
*  that pay excessively high fees.
*/
struct limit_fee_operation
{
    static const operation_type_enum type;

    asset max_fee;

    void evaluate( transaction_evaluation_state& eval_state )const;
};

} } // bts::blockchain

FC_REFLECT( bts::blockchain::withdraw_operation, (balance_id)(amount)(claim_input_data) )
FC_REFLECT( bts::blockchain::deposit_operation, (amount)(condition) )
FC_REFLECT( bts::blockchain::burn_operation, (amount)(account_id)(message)(message_signature) )
FC_REFLECT( bts::blockchain::release_escrow_operation, (escrow_id)(released_by)(amount_to_receiver)(amount_to_sender) )
FC_REFLECT( bts::blockchain::update_balance_vote_operation, (balance_id)(new_restricted_owner)(new_slate) )
FC_REFLECT( bts::blockchain::limit_fee_operation, (max_fee) )
