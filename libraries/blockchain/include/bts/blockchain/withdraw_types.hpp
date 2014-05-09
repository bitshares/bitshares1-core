#pragma once
#include <fc/io/raw.hpp>
#include <bts/blockchain/pts_address.hpp>
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/asset.hpp>

namespace bts { namespace blockchain {

   enum withdraw_condition_types 
   {
      /** assumes blockchain already knowws the condition, which
       * is provided the first time something is withdrawn */
      withdraw_null_type        = 0,
      withdraw_signature_type   = 1,
      withdraw_multi_sig_type   = 2,
      withdraw_password_type    = 3,
      withdraw_option_type      = 4,
   };

   /**
    * The withdraw condition defines which delegate this address 
    * is voting for, assuming asset_type == 0
    */
   struct withdraw_condition
   {
      withdraw_condition():asset_id(0),delegate_id(0),condition(0){}

      template<typename WithdrawType>
      withdraw_condition( const WithdrawType& t, asset_id_type asset_id_arg = 0, name_id_type delegate_id_arg = 0 )
      {
         condition = WithdrawType::type;
         asset_id = asset_id_arg;
         delegate_id = delegate_id_arg;
         data = fc::raw::pack( t );
      }

      template<typename WithdrawType>
      WithdrawType as()const
      {
         FC_ASSERT( condition == WithdrawType::type, "", ("condition",condition)("WithdrawType",WithdrawType::type) );
         return fc::raw::unpack<WithdrawType>(data);
      }

      account_id_type get_account()const;

      asset_id_type     asset_id;
      name_id_type      delegate_id;
      fc::enum_type<uint8_t,withdraw_condition_types>           condition;
      std::vector<char> data;
   };

   struct withdraw_with_signature 
   {
      static const uint8_t    type;
      withdraw_with_signature( const address owner_arg = address() )
      :owner(owner_arg){}

      address                 owner; 
   };


   struct withdraw_with_multi_sig
   {
      static const uint8_t              type;

      uint32_t             required;
      std::vector<address> owners; 
   };

   struct withdraw_with_password
   {
      static const uint8_t type;

      address payee;
      address payor;
      fc::ripemd160        password_hash;
   };

   struct withdraw_option
   {
      static const uint8_t type;

      address              optionor; 
      address              optionee;
      fc::time_point_sec   date;
      price                strike_price;
   };


} } // bts::blockchain


namespace fc {
   void to_variant( const bts::blockchain::withdraw_condition& var,  variant& vo );
   void from_variant( const variant& var,  bts::blockchain::withdraw_condition& vo );
}

FC_REFLECT_ENUM( bts::blockchain::withdraw_condition_types, 
        (withdraw_null_type)
        (withdraw_signature_type)
        (withdraw_multi_sig_type)
        (withdraw_password_type)
        (withdraw_option_type) )

FC_REFLECT( bts::blockchain::withdraw_condition, (asset_id)(delegate_id)(condition)(data) )
FC_REFLECT( bts::blockchain::withdraw_with_signature, (owner) )
FC_REFLECT( bts::blockchain::withdraw_with_multi_sig, (required)(owners) )
FC_REFLECT( bts::blockchain::withdraw_with_password, (payee)(payor)(password_hash) )
FC_REFLECT( bts::blockchain::withdraw_option, (optionor)(optionee)(date)(strike_price) )

