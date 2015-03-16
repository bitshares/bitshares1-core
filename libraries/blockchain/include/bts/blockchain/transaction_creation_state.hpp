#pragma once

#include <bts/blockchain/pending_chain_state.hpp>
#include <bts/blockchain/transaction_evaluation_state.hpp>

namespace bts { namespace blockchain {

/**
 *  Transaction creation state is used to build up a transaction
 *  to perform a set of operations given an intial subset of the
 *  blockchain state (pending_state)
 *
 *  When building a transaction that withdraws from balances, orders,
 *  or otherwise the key must be in specified via add_known_key(addr)
 *  prior to performing the operation.  Otherwise, the operation will
 *  fail.
 *
 *
 */
class transaction_creation_state
{
   public:
      transaction_creation_state( chain_interface_ptr prev_state = chain_interface_ptr() );

      /** Adds the key to the set of known keys for the purpose of
       * building this transaction.  Only balances controlled by keys
       * specified in this way will be considered.
       */
      void add_known_key( const address& addr );

      /** Withdraw from any balance in pending_state._balance_id_to_record that
       * is of the proper type and for which the owenr key is known and has been
       * specified via add_known_key()
       */
      void withdraw( const asset& amount );

      public_key_type deposit( const asset&                      amount,
                               const public_key_type&            to,
                               slate_id_type                     slate   = 0,
                               const optional<private_key_type>& one_time_key = optional<private_key_type>(),
                               const string&                     memo    = string(),
                               const optional<private_key_type>& from    = optional<private_key_type>(),
                               bool                              stealth = false );

      signed_transaction           trx;
      pending_chain_state          pending_state;
      vector<multisig_condition>   required_signatures;
      transaction_evaluation_state eval_state;
};

} } // bts::blockchain

FC_REFLECT( bts::blockchain::transaction_creation_state,
            (trx)
            (pending_state)
            (required_signatures)
            (eval_state)
            )
