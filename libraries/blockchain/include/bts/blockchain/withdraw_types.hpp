#pragma once

#include <bts/blockchain/asset.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/types.hpp>

#include <fc/io/enum_type.hpp>
#include <fc/io/raw.hpp>

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
      withdraw_escrow_type      = 5,
      withdraw_vesting_type     = 6
   };

   /**
    * The withdraw condition defines which delegate this address
    * is voting for, assuming asset_type == 0
    */
   struct withdraw_condition
   {
      withdraw_condition():asset_id(0),delegate_slate_id(0),type(withdraw_null_type){}

      template<typename WithdrawType>
      withdraw_condition( const WithdrawType& t, asset_id_type asset_id_arg = 0, slate_id_type delegate_id_arg = 0 )
      {
         type = WithdrawType::type;
         asset_id = asset_id_arg;
         delegate_slate_id = delegate_id_arg;
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
      slate_id_type                                     delegate_slate_id = 0;
      fc::enum_type<uint8_t, withdraw_condition_types>  type = withdraw_null_type;
      std::vector<char>                                 data;
   };

   enum memo_flags_enum
   {
      from_memo = 0, ///< default memo type, the public key is who the deposit is from
      /**
       *  Alternative memo type, the public key is who the deposit is to.  This
       *  can be used for 'change' memos and help the sender reconstruct information
       *  in the event they lose their wallet.
       */
      to_memo = 1,
   };

   struct memo_data
   {
      public_key_type                      from;
      uint64_t                             from_signature;

      void        set_message( const std::string& message );
      std::string get_message()const;

      /** messages are a constant length to preven analysis of
       * transactions with the same length memo_data
       */
      fc::array<char,BTS_BLOCKCHAIN_MAX_MEMO_SIZE>     message;
      fc::enum_type<uint8_t,memo_flags_enum>           memo_flags;
   };
   typedef fc::optional<memo_data>         omemo_data;

   struct memo_status : public memo_data
   {
      memo_status():has_valid_signature(false){}

      memo_status( const memo_data& memo,
                   bool valid_signature,
                   const fc::ecc::private_key& opk );

      bool                 has_valid_signature;
      fc::ecc::private_key owner_private_key;
   };
   typedef fc::optional<memo_status> omemo_status;

   struct titan_memo
   {
      public_key_type   one_time_key;
      vector<char>      encrypted_memo_data;
   };

   struct withdraw_with_signature
   {
      static const uint8_t    type;
      withdraw_with_signature( const address owner_arg = address() )
      :owner(owner_arg){}

      omemo_status decrypt_memo_data( const fc::ecc::private_key& receiver_key )const;
      void         encrypt_memo_data( const fc::ecc::private_key& one_time_private_key,
                                      const fc::ecc::public_key&  to_public_key,
                                      const fc::ecc::private_key& from_private_key,
                                      const std::string& memo_message,
                                      const fc::ecc::public_key&  memo_pub_key,
                                      memo_flags_enum memo_type = from_memo);

      memo_data    decrypt_memo_data( const fc::sha512& secret )const;
      void         encrypt_memo_data( const fc::sha512& secret, const memo_data& );

      address                 owner;
      optional<titan_memo>    memo;
   };

   struct withdraw_with_multi_sig
   {
      static const uint8_t    type;

      uint32_t                required;
      std::vector<address>    owners;
      optional<titan_memo>    memo;
   };

   struct withdraw_with_escrow
   {
      static const uint8_t    type;

      address                 sender;
      address                 receiver;
      address                 escrow;
      digest_type             agreement_digest;
      optional<titan_memo>    memo;
   };

   struct withdraw_with_pts
   {
      public_key_type             new_key;
      fc::ecc::compact_signature  pts_signature;
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

      address                         payee;
      address                         payor;
      fc::time_point_sec              timeout;
      fc::ripemd160                   password_hash;
      optional<titan_memo>            memo;
   };

   struct withdraw_option
   {
      static const uint8_t type;

      address              optionor;
      address              optionee;
      fc::time_point_sec   date;
      price                strike_price;
   };

   struct withdraw_vesting
   {
       static const uint8_t    type;

       address                 owner;
       fc::time_point_sec      vesting_start;
       uint32_t                vesting_duration;
       share_type              claimed;
       share_type              total;
   };

} } // bts::blockchain

namespace fc {
   void to_variant( const bts::blockchain::withdraw_condition& var,  variant& vo );
   void from_variant( const variant& var,  bts::blockchain::withdraw_condition& vo );
   void to_variant( const bts::blockchain::memo_data& var,  variant& vo );
   void from_variant( const variant& var,  bts::blockchain::memo_data& vo );
}

FC_REFLECT_ENUM( bts::blockchain::withdraw_condition_types,
                 (withdraw_null_type)
                 (withdraw_signature_type)
                 (withdraw_multi_sig_type)
                 (withdraw_password_type)
                 (withdraw_option_type) )

FC_REFLECT( bts::blockchain::titan_memo, (one_time_key)(encrypted_memo_data) );
FC_REFLECT( bts::blockchain::withdraw_condition, (asset_id)(delegate_slate_id)(type)(data) )
FC_REFLECT( bts::blockchain::withdraw_with_signature, (owner)(memo) )
FC_REFLECT( bts::blockchain::withdraw_with_multi_sig, (required)(owners)(memo) )
FC_REFLECT( bts::blockchain::withdraw_with_password, (payee)(payor)(timeout)(password_hash)(memo) )
FC_REFLECT( bts::blockchain::withdraw_option, (optionor)(optionee)(date)(strike_price) )
FC_REFLECT( bts::blockchain::withdraw_with_pts, (new_key)(pts_signature) )
FC_REFLECT( bts::blockchain::withdraw_vesting, (owner)(vesting_start)(vesting_duration)(claimed)(total) )
FC_REFLECT_ENUM( bts::blockchain::memo_flags_enum, (from_memo)(to_memo) )
FC_REFLECT( bts::blockchain::memo_data, (from)(from_signature)(message)(memo_flags) );
FC_REFLECT_DERIVED( bts::blockchain::memo_status,
                    (bts::blockchain::memo_data),
                    (has_valid_signature)(owner_private_key) )
FC_REFLECT( bts::blockchain::withdraw_with_escrow, (sender)(receiver)(escrow)(agreement_digest)(memo) )

