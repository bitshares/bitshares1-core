#pragma once 
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/transaction.hpp>
#include <bts/blockchain/asset.hpp>

namespace bts { namespace blockchain { 

   class chain_interface;
   typedef shared_ptr<chain_interface> chain_interface_ptr;

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
         transaction_evaluation_state( const chain_interface_ptr& blockchain, digest_type chain_id );
         transaction_evaluation_state(){};

         virtual ~transaction_evaluation_state();
         virtual share_type get_fees( asset_id_type id = 0)const;

         virtual void reset();
         
         virtual void evaluate( const signed_transaction& trx );
         virtual void evaluate_operation( const operation& op );

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
         
         bool check_signature( const address& a )const;
        
         /**
          *  subtracts amount from a withdraw_with_signature account with the
          *  owner_key and amount.asset_id and the delegate_id of the transaction.
          */
         void add_required_deposit( const address& owner_key, const asset& amount );
         
         /** contains address funds were deposited into for use in
          * incrementing required_deposits balance
          */
         void sub_balance( const balance_id_type& addr, const asset& amount );
         void add_balance( const asset& amount );
         
         /** any time a balance is deposited increment the vote for the delegate,
          * if delegate_id is negative then it is a vote against abs(delegate_id)
          */
         void add_vote( name_id_type delegate_id, share_type amount );
         void sub_vote( name_id_type delegate_id, share_type amount );

         void validate_asset( const asset& a )const;
         
         signed_transaction                          trx;
         unordered_set<address>                      signed_keys;
         
         // increases with funds are withdrawn, decreases when funds are deposited or fees paid
         optional<fc::exception>                     validation_error;
         
         
         /** every time a deposit is made this balance is increased
          *  every time a deposit is required this balance is decreased
          *
          *  This balance cannot be negative without an error.
          */
         unordered_map<balance_id_type, asset>       required_deposits;
         unordered_map<balance_id_type, asset>       provided_deposits;

         // track deposits and withdraws by asset type
         unordered_map<asset_id_type, asset>         deposits;
         unordered_map<asset_id_type, asset>         withdraws;

         asset                                       required_fees;
         
         /**
          *  As operation withdraw funds, input balance grows...
          *  As operations consume funds (deposit) input balance decreases
          *
          *  Any left-over input balance can be seen as fees
          *
          *  @note - this value should always equal the sum of deposits-withdraws 
          *  and is maintained for the purpose of seralization.
          */
         unordered_map<asset_id_type, share_type>    balance;


         struct vote_state
         {
            vote_state():votes_for(0),votes_against(0){}
         
            int64_t votes_for;
            int64_t votes_against;
         };
         /**
          *  Tracks the votes for or against each delegate based upon 
          *  the deposits and withdraws to addresses.
          */
         unordered_map<name_id_type, vote_state>     net_delegate_votes;


      // not serialized
         chain_interface_ptr                              _current_state;
         digest_type                                      _chain_id;
   };

   typedef shared_ptr<transaction_evaluation_state> transaction_evaluation_state_ptr;

} } // bts::blockchain

FC_REFLECT( bts::blockchain::transaction_evaluation_state::vote_state, (votes_for)(votes_against) )
FC_REFLECT( bts::blockchain::transaction_evaluation_state, 
           (trx)(signed_keys)
           (validation_error)
           (required_deposits)
           (provided_deposits)
           (deposits)(withdraws)(balance)(net_delegate_votes)(balance) )
