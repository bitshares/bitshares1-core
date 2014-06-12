#pragma once 
#include <bts/blockchain/address.hpp>
#include <bts/blockchain/withdraw_types.hpp>
#include <fc/time.hpp>

/**
 *  The C keyword 'not' is NOT friendly on VC++ but we still want to use
 *  it for readability, so we will have the pre-processor convert it to the
 *  more traditional form.  The goal here is to make the understanding of
 *  the validation logic as english-like as possible.
 */
#define NOT !

namespace bts { namespace blockchain {

   class transaction_evaluation_state;

   enum operation_type_enum
   {
      null_op_type                = 0,

      /** balance operations */
      withdraw_op_type            = 1,
      deposit_op_type             = 2,

      /** account operations */
      register_account_op_type    = 3,
      update_account_op_type      = 4,
      withdraw_pay_op_type        = 5,

      /** asset operations */
      create_asset_op_type        = 6,
      update_asset_op_type        = 7,
      issue_asset_op_type         = 8,

      /** delegate operations */
      fire_delegate_op_type       = 9,

      /** proposal operations */
      submit_proposal_op_type     = 10,
      vote_proposal_op_type       = 11,

      /** market operations */
      bid_op_type                 = 12,
      ask_op_type                 = 13,
      short_op_type               = 14,
      cover_op_type               = 15,
      add_collateral_op_type      = 16,
      remove_collateral_op_type   = 17
   };

   /**
    *  A poly-morphic operator that modifies the blockchain database 
    *  is some manner.
    */
   struct operation
   {
      operation():type(null_op_type){}

      operation( const operation& o )
      :type(o.type),data(o.data){}

      operation( operation&& o )
      :type(o.type),data(std::move(o.data)){}

      template<typename OperationType>
      operation( const OperationType& t )
      {
         type = OperationType::type;
         data = fc::raw::pack( t );
      }

      template<typename OperationType>
      OperationType as()const
      {
         FC_ASSERT( (operation_type_enum)type == OperationType::type, "", ("type",type)("OperationType",OperationType::type) );
         return fc::raw::unpack<OperationType>(data);
      }

      operation& operator=( const operation& o )
      {
         if( this == &o ) return *this;
         type = o.type;
         data = o.data;
         return *this;
      }

      operation& operator=( operation&& o )
      {
         if( this == &o ) return *this;
         type = o.type;
         data = std::move(o.data);
         return *this;
      }

      fc::enum_type<uint8_t,operation_type_enum> type;
      std::vector<char> data;
   };


} } // bts::blockchain

FC_REFLECT_ENUM( bts::blockchain::operation_type_enum,
                 (null_op_type)
                 (withdraw_op_type)
                 (deposit_op_type)
                 (create_asset_op_type)
                 (update_asset_op_type)
                 (withdraw_pay_op_type)
                 (register_account_op_type)
                 (update_account_op_type)
                 (issue_asset_op_type)
                 (submit_proposal_op_type)
                 (vote_proposal_op_type)
                 (bid_op_type)
                 (ask_op_type)
                 (short_op_type)
                 (cover_op_type)
                 (add_collateral_op_type)
                 (remove_collateral_op_type)
               )

FC_REFLECT( bts::blockchain::operation, (type)(data) )
 
namespace fc {
   void to_variant( const bts::blockchain::operation& var,  variant& vo );
   void from_variant( const variant& var,  bts::blockchain::operation& vo );
}

