#pragma once
#include <bts/blockchain/operations.hpp>

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
       std::vector<char> claim_input_data;


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
       deposit_operation( const address& owner, const asset& amnt, name_id_type delegate_id );

       /** the condition that the funds may be withdrawn,
        *  this is only necessary if the address is new.
        */
       share_type                       amount;
       withdraw_condition               condition;

       void evaluate( transaction_evaluation_state& eval_state );
   };

} } // bts::blockchain 

FC_REFLECT( bts::blockchain::withdraw_operation, (balance_id)(amount)(claim_input_data) )
FC_REFLECT( bts::blockchain::deposit_operation, (amount)(condition) )
