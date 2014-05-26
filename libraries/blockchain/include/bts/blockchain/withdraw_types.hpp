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
      withdraw_by_name_type     = 5
   };

   /**
    * The withdraw condition defines which delegate this address 
    * is voting for, assuming asset_type == 0
    */
   struct withdraw_condition
   {
      withdraw_condition():asset_id(0),delegate_id(0),type(withdraw_null_type){}

      template<typename WithdrawType>
      withdraw_condition( const WithdrawType& t, asset_id_type asset_id_arg = 0, name_id_type delegate_id_arg = 0 )
      {
         type = WithdrawType::type;
         asset_id = asset_id_arg;
         delegate_id = delegate_id_arg;
         data = fc::raw::pack( t );
      }

      template<typename WithdrawType>
      WithdrawType as()const
      {
         FC_ASSERT( type == WithdrawType::type, "", ("type",type)("WithdrawType",WithdrawType::type) );
         return fc::raw::unpack<WithdrawType>(data);
      }

      balance_id_type get_address()const;

      asset_id_type                                     asset_id;
      name_id_type                                      delegate_id;
      fc::enum_type<uint8_t,withdraw_condition_types>   type;
      std::vector<char>                                 data;
   };

   struct withdraw_with_signature 
   {
      static const uint8_t    type;
      withdraw_with_signature( const address owner_arg = address() )
      :owner(owner_arg){}

      address                 owner; 
   };

   struct memo_data
   {
      /** 
       * @note using address rather than name_id because a name
       * can change their active key and there can be unregistered
       * names that may wish to be identified.
       */
      address                              from_address;
      uint64_t                             from_signature;

      /** messages are a constant length to preven analysis of
       * transactions with the same length memo_data
       */
      fc::optional<fc::array<char,20>>     message;
   };

   struct withdraw_by_name
   {
      static const uint8_t    type;
      withdraw_by_name( const address owner_arg = address() )
      :owner(owner_arg){}

      memo_data decrypt_memo_data( const fc::sha512& secret )const;
      void      encrypt_memo_data( const fc::sha512& secret, const memo_data& );

      public_key_type         one_time_key;
      std::vector<char>       encrypted_memo_data;
      address                 owner; 
   };


   struct withdraw_with_multi_sig
   {
      static const uint8_t              type;

      uint32_t             required;
      std::vector<address> owners; 
   };

   /**
    *  User A picks a random password and generates password_hash. 
    *  User A sends funds to user B which they may claim with the password + their signature, but
    *     where User A can recover the funds after a timeout T.
    *  User B sends funds to user A under the same conditions where they can recover the
    *     the funds after a timeout T2 << T
    *  User A claims the funds from User B revealing password before T2.... 
    *  User B now has the time between T2 and T to claim the funds.
    *
    *  When User A claims the funds from user B, user B learns the password that
    *  allows them to spend the funds from user A.  
    *
    *  User A can spend the funds after a timeout.  
    */
   struct withdraw_with_password
   {
      static const uint8_t type;

      address              payee;
      address              payor;
      fc::time_point_sec   timeout;
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
        (withdraw_option_type) 
        (withdraw_by_name_type) 
        )

FC_REFLECT( bts::blockchain::withdraw_condition, (asset_id)(delegate_id)(type)(data) )
FC_REFLECT( bts::blockchain::withdraw_with_signature, (owner) )
FC_REFLECT( bts::blockchain::withdraw_with_multi_sig, (required)(owners) )
FC_REFLECT( bts::blockchain::withdraw_with_password, (payee)(payor)(timeout)(password_hash) )
FC_REFLECT( bts::blockchain::withdraw_option, (optionor)(optionee)(date)(strike_price) )
FC_REFLECT( bts::blockchain::withdraw_by_name, (one_time_key)(encrypted_memo_data)(owner) )
FC_REFLECT( bts::blockchain::memo_data, (from_address)(from_signature)(message) );

