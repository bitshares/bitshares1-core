#pragma once

#include <bts/blockchain/condition.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/types.hpp>

namespace bts { namespace blockchain {

class pending_chain_state;
typedef std::shared_ptr<pending_chain_state> pending_chain_state_ptr;

/**
*  While evaluating a transaction there is a lot of intermediate
*  state that must be tracked.  Any shares withdrawn from the
*  database must be stored in the transaction state until they
*  are sent back to the database as either new balances or
*  as fees collected.
*
*  Some outputs such as markets, options, etc require certain
*  payments to be made.  So payments made are tracked and
*  compared against payments required.
*
*/
struct transaction_evaluation_state
{
    transaction_evaluation_state( pending_chain_state_ptr pending_state = nullptr ) : _pending_state( pending_state ) {}

    pending_chain_state* pending_state()const
    {
        const pending_chain_state_ptr ptr = _pending_state.lock();
        FC_ASSERT( ptr );
        return ptr.get();
    }

    void evaluate( const signed_transaction& trx );
    void evaluate_operation( const operation& op );

    bool check_signature( const address& a )const;
    bool check_multisig( const multisig_condition& a )const;
    bool verify_authority( const multisig_meta_info& siginfo )const;

    bool any_parent_has_signed( const string& account_name )const;
    bool account_or_any_parent_has_signed( const account_record& record )const;

    void add_balance( const asset& amount );
    void sub_balance( const asset& amount );

    void adjust_vote( slate_id_type slate, share_type amount );

    bool scan_op_deltas( const uint32_t op_index, const function<bool( const asset& )> callback )const;
    void scan_addresses( const chain_interface&, const function<void( const address& )> callback )const;

    signed_transaction                             trx;
    set<address>                                   signed_addresses;

    map<asset_id_type, share_type>                 min_fees; // For asset and delegate registration
    map<asset_id_type, share_type>                 max_fees; // For pay_fee op

    map<asset_id_type, share_type>                 fees_paid;
    share_type                                     total_base_equivalent_fees_paid = 0;

    map<account_id_type, share_type>               delegate_vote_deltas;

    // Used for transaction scanning
    map<asset_id_type, share_type>                 yield_claimed;
    map<uint32_t, map<asset_id_type, share_type>>  op_deltas;

    optional<fc::exception>                        validation_error;

    // Below not serialized
    bool                                           _skip_signature_check = false;
    bool                                           _enforce_canonical_signatures = false;
    bool                                           _skip_vote_adjustment = false;

private:
    std::weak_ptr<pending_chain_state>             _pending_state;
    uint32_t                                       _current_op_index = 0;

    void validate_fees();
    void calculate_base_equivalent_fees();
    void update_delegate_votes();
};
typedef shared_ptr<transaction_evaluation_state> transaction_evaluation_state_ptr;

} } // bts::blockchain

FC_REFLECT( bts::blockchain::transaction_evaluation_state,
            (trx)
            (signed_addresses)
            (min_fees)
            (max_fees)
            (fees_paid)
            (total_base_equivalent_fees_paid)
            (delegate_vote_deltas)
            (yield_claimed)
            (op_deltas)
            (validation_error)
            )
