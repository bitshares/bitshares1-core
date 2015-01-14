#pragma once

#include <bts/blockchain/condition.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/types.hpp>

namespace bts { namespace blockchain {

    class pending_chain_state;

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
   class transaction_evaluation_state
   {
      public:
         transaction_evaluation_state( pending_chain_state* current_state = nullptr );

         virtual ~transaction_evaluation_state();
         virtual share_type get_fees( asset_id_type id = 0)const;

         virtual void evaluate( const signed_transaction& trx, bool skip_signature_check = false, bool enforce_canonical = false );
         virtual void evaluate_operation( const operation& op );
         virtual bool verify_authority( const multisig_meta_info& siginfo );

         /** perform any final operations based upon the current state of
          * the operation such as updating fees paid etc.
          */
         virtual void post_evaluate();

         /** can be specalized to handle many different forms of
          * fee payment.
          */
         virtual void validate_required_fee();

         /**
          * apply collected vote changes
          */
         virtual void update_delegate_votes();
         virtual void verify_delegate_id( const account_id_type id )const;

         bool check_signature( const address& a )const;
         bool check_multisig( const multisig_condition& a )const;
         bool check_update_permission( const object_id_type id )const;

         bool any_parent_has_signed( const string& account_name )const;
         bool account_or_any_parent_has_signed( const account_record& record )const;

         void sub_balance( const balance_id_type& addr, const asset& amount );
         void add_balance( const asset& amount );

         /** any time a balance is deposited increment the vote for the delegate,
          * if delegate_id is negative then it is a vote against abs(delegate_id)
          */
         void adjust_vote( slate_id_type slate, share_type amount );

         void validate_asset( const asset& a )const;

         bool scan_deltas( const uint32_t op_index, const function<bool( const asset& )> callback )const;

         void scan_addresses( const chain_interface&, const function<void( const address& )> callback )const;

         signed_transaction                             trx;

         set<address>                                   signed_keys;

         // increases with funds are withdrawn, decreases when funds are deposited or fees paid
         optional<fc::exception>                        validation_error;

         // track deposits and withdraws by asset type
         unordered_map<asset_id_type, share_type>       deposits;
         unordered_map<asset_id_type, share_type>       withdraws;
         unordered_map<asset_id_type, share_type>       yield;

         map<uint32_t, map<asset_id_type, share_type>>  deltas;

         asset                                          required_fees;
         /**
          *  The total fees paid by in alternative asset types (like BitUSD) calculated
          *  by using the median feed price
          */
         asset                                          alt_fees_paid;

         /**
          *  As operation withdraw funds, input balance grows...
          *  As operations consume funds (deposit) input balance decreases
          *
          *  Any left-over input balance can be seen as fees
          *
          *  @note - this value should always equal the sum of deposits-withdraws
          *  and is maintained for the purpose of seralization.
          */
         map<asset_id_type, share_type>                 balance;

         /**
          *  Tracks the votes for or against each delegate based upon
          *  the deposits and withdraws to addresses.
          */
         unordered_map<account_id_type, int64_t>        net_delegate_votes;

         // Not serialized
         chain_interface*                               _current_state = nullptr;
         bool                                           _skip_signature_check = false;
         unordered_map<asset_id_type, share_type>       _max_fee;
         uint32_t                                       current_op_index = 0;
   };
   typedef shared_ptr<transaction_evaluation_state> transaction_evaluation_state_ptr;

} } // bts::blockchain

FC_REFLECT( bts::blockchain::transaction_evaluation_state,
           (trx)
           (signed_keys)
           (validation_error)
           (deposits)
           (withdraws)
           (yield)
           (deltas)
           (required_fees)
           (alt_fees_paid)
           (balance)
           (net_delegate_votes)
          )
