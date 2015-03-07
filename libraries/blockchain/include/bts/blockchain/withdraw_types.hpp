#pragma once

#include <bts/blockchain/asset.hpp>
#include <bts/blockchain/config.hpp>
#include <bts/blockchain/types.hpp>

#include <fc/io/enum_type.hpp>
#include <fc/io/raw.hpp>

namespace bts { namespace blockchain {

   enum withdraw_condition_types
   {
      withdraw_null_type        = 0,
      withdraw_signature_type   = 1,
      withdraw_vesting_type     = 2,
      withdraw_multisig_type    = 3,
      withdraw_escrow_type      = 6
   };

   struct withdraw_condition
   {
      withdraw_condition():asset_id(0),slate_id(0),type(withdraw_null_type){}

      template<typename WithdrawType>
      withdraw_condition( const WithdrawType& t, asset_id_type asset_id_arg = 0, slate_id_type delegate_id_arg = 0 )
      {
         type = WithdrawType::type;
         asset_id = asset_id_arg;
         slate_id = delegate_id_arg;
         data = fc::raw::pack( t );
      }

      template<typename WithdrawType>
      WithdrawType as()const
      {
         FC_ASSERT( type == WithdrawType::type, "", ("type",type)("WithdrawType",WithdrawType::type) );
         return fc::raw::unpack<WithdrawType>(data);
      }

      balance_id_type   get_address()const;
      set<address>      owners()const;
      optional<address> owner()const;
      string            type_label()const;

      asset_id_type                                     asset_id;
      slate_id_type                                     slate_id = 0;
      fc::enum_type<uint8_t, withdraw_condition_types>  type = withdraw_null_type;
      std::vector<char>                                 data;
   };

   struct titan_memo
   {
      public_key_type   one_time_key;
      vector<char>      encrypted_memo_data;
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
      uint64_t                             from_signature = 0;

      void        set_message( const std::string& message );
      std::string get_message()const;

      /** messages are a constant length to preven analysis of
       * transactions with the same length memo_data
       */
      fc::array<char,BTS_BLOCKCHAIN_MAX_MEMO_SIZE>     message;
      fc::enum_type<uint8_t,memo_flags_enum>           memo_flags = from_memo;
   };
   typedef fc::optional<memo_data>         omemo_data;

   /** The purpose of the extended memo data is to support extra
    * message data beyond the 19 bytes afforded by the default
    * message.
    */
   struct extended_memo_data
   {
      public_key_type                      from;
      uint64_t                             from_signature = 0;

      void        set_message( const std::string& message );
      std::string get_message()const;

      /** messages are a constant length to preven analysis of
       * transactions with the same length memo_data
       */
      fc::array<char,BTS_BLOCKCHAIN_MAX_MEMO_SIZE>      message;
      fc::enum_type<uint8_t,memo_flags_enum>            memo_flags = from_memo;
      fc::array<char,BTS_BLOCKCHAIN_EXTENDED_MEMO_SIZE> extra_message;
   };
   typedef fc::optional<extended_memo_data> oextended_memo_data;

   struct memo_status : public extended_memo_data
   {
      memo_status(){}
      memo_status( const extended_memo_data& memo,
                   bool valid_signature,
                   const fc::ecc::private_key& opk );

      bool                 has_valid_signature = false;
      fc::ecc::private_key owner_private_key;
   };
   typedef fc::optional<memo_status> omemo_status;

   struct withdraw_with_signature
   {
      static const uint8_t    type;

      withdraw_with_signature( const address owner_arg = address() )
      :owner(owner_arg){}

      omemo_status     decrypt_memo_data( const fc::ecc::private_key& receiver_key, bool ignore_owner = false )const;
      public_key_type  encrypt_memo_data( const fc::ecc::private_key& one_time_private_key,
                                          const fc::ecc::public_key&  to_public_key,
                                          const fc::ecc::private_key& from_private_key,
                                          const std::string& memo_message,
                                          bool use_stealth_address = false );

      extended_memo_data decrypt_memo_data( const fc::sha512& secret )const;
      void         encrypt_memo_data( const fc::sha512& secret, const memo_data& );
      void         encrypt_memo_data( const fc::sha512& secret, const extended_memo_data& );

      address                 owner;
      optional<titan_memo>    memo;
   };

   struct withdraw_vesting
   {
       static const uint8_t type;

       address              owner;
       fc::time_point_sec   start_time;
       uint32_t             duration = 0;
       share_type           original_balance = 0;
   };

   struct withdraw_with_multisig
   {
      static const uint8_t    type;

      uint32_t                required;
      set<address>            owners;
      optional<titan_memo>    memo;
   };

   struct withdraw_with_escrow
   {
      static const uint8_t    type;

      address                 sender;
      address                 receiver;
      address                 escrow;
      digest_type             agreement_digest;

      omemo_status decrypt_memo_data( const fc::ecc::private_key& receiver_key, bool ignore_owner = false )const;
      public_key_type encrypt_memo_data( const fc::ecc::private_key& one_time_private_key,
                                         const fc::ecc::public_key&  to_public_key,
                                         const fc::ecc::private_key& from_private_key,
                                         const std::string& memo_message );

      extended_memo_data    decrypt_memo_data( const fc::sha512& secret )const;
      void                  encrypt_memo_data( const fc::sha512& secret, const extended_memo_data& );
      void                  encrypt_memo_data( const fc::sha512& secret, const memo_data& );

      optional<titan_memo>    memo;
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
        (withdraw_vesting_type)
        (withdraw_multisig_type)
        (withdraw_escrow_type)
        )
FC_REFLECT( bts::blockchain::withdraw_condition,
        (asset_id)
        (slate_id)
        (type)
        (data)
        )
FC_REFLECT( bts::blockchain::titan_memo,
        (one_time_key)
        (encrypted_memo_data)
        )
FC_REFLECT_ENUM( bts::blockchain::memo_flags_enum,
        (from_memo)
        (to_memo)
        )
FC_REFLECT( bts::blockchain::memo_data,
        (from)
        (from_signature)
        (message)
        (memo_flags)
        )

FC_REFLECT( bts::blockchain::extended_memo_data,
        (from)
        (from_signature)
        (message)
        (memo_flags)
        (extra_message)
        )
FC_REFLECT_DERIVED( bts::blockchain::memo_status,
        (bts::blockchain::memo_data),
        (has_valid_signature)
        (owner_private_key)
        )
FC_REFLECT( bts::blockchain::withdraw_with_signature,
        (owner)
        (memo)
        )
FC_REFLECT( bts::blockchain::withdraw_vesting,
        (owner)
        (start_time)
        (duration)
        (original_balance)
        )
FC_REFLECT( bts::blockchain::withdraw_with_multisig,
        (required)
        (owners)
        (memo)
        )
FC_REFLECT( bts::blockchain::withdraw_with_escrow,
        (sender)
        (receiver)
        (escrow)
        (agreement_digest)
        (memo)
        )
