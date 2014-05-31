#pragma once 
#include <bts/blockchain/address.hpp>
#include <bts/blockchain/withdraw_types.hpp>
#include <fc/time.hpp>

namespace bts { namespace blockchain {

   enum operation_type_enum
   {
      null_op_type                = 0,

      /** balance operations */
      withdraw_op_type            = 1,
      deposit_op_type             = 2,

      /** account operations */
      register_account_op_type        = 3,
      update_account_op_type         = 4,

      /** asset operations */
      create_asset_op_type        = 5,
      update_asset_op_type        = 6,
      issue_asset_op_type         = 7,

      /** delegate operations */
      fire_delegate_op_type       = 8,

      /** proposal operations */
      submit_proposal_op_type     = 9,
      vote_proposal_op_type       = 10,

      /** market operations */
      bid_op_type                 = 11,
      ask_op_type                 = 12,
      short_op_type               = 13,
      cover_op_type               = 14,
      add_collateral_op_type      = 15,
      remove_collateral_op_type   = 16
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

#include <bts/blockchain/balance_operations.hpp>
#include <bts/blockchain/asset_operations.hpp>
#include <bts/blockchain/account_operations.hpp>
#include <bts/blockchain/market_operations.hpp>
#include <bts/blockchain/proposal_operations.hpp>
