#pragma once

#include <bts/blockchain/asset.hpp>
#include <bts/blockchain/delegate_slate.hpp>
#include <bts/blockchain/operations.hpp>
#include <bts/blockchain/withdraw_types.hpp>

namespace bts { namespace blockchain {

   /**
    *  Declares a set of delegates to vote for or against
    */
   struct define_delegate_slate_operation
   {
      static const operation_type_enum type;

      define_delegate_slate_operation(){}
      define_delegate_slate_operation( delegate_slate s )
      :slate( std::move(s) ){}

      delegate_slate slate;

      void evaluate( transaction_evaluation_state& eval_state );
   };

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

       void evaluate( transaction_evaluation_state& eval_state );
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

       void evaluate( transaction_evaluation_state& eval_state );
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

       void evaluate( transaction_evaluation_state& eval_state );
   };


} } // bts::blockchain

FC_REFLECT( bts::blockchain::define_delegate_slate_operation, (slate) )
FC_REFLECT( bts::blockchain::withdraw_operation, (balance_id)(amount)(claim_input_data) )
FC_REFLECT( bts::blockchain::deposit_operation, (amount)(condition) )
FC_REFLECT( bts::blockchain::burn_operation, (amount)(account_id)(message)(message_signature) )
