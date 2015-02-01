#pragma once

#include <bts/blockchain/account_record.hpp>
#include <bts/blockchain/operations.hpp>

namespace bts { namespace blockchain {

   struct register_account_operation
   {
      static const operation_type_enum type;

      register_account_operation(){}

      register_account_operation( const std::string& n,
                                  const fc::variant& d,
                                  const public_key_type& owner,
                                  const public_key_type& active,
                                  uint8_t pay_rate = -1 )
      :name(n),public_data(d),owner_key(owner),active_key(active),delegate_pay_rate(pay_rate){}

      std::string                 name;
      fc::variant                 public_data;
      public_key_type             owner_key;
      public_key_type             active_key;

      uint8_t                     delegate_pay_rate = -1;

      /**
       *  Meta information is used by clients to evaluate
       *  how to send to this account.  This is designed to
       *  support registering of multi-sig accounts or
       *  accounts with other advanced rules.
       *
       *  This data does not effect validation rules.
       */
      optional<account_meta_info> meta_data;

      bool is_delegate()const;

      void evaluate( transaction_evaluation_state& eval_state )const;
      void evaluate_v1( transaction_evaluation_state& eval_state )const;
   };

   struct update_account_operation
   {
      static const operation_type_enum type;

      account_id_type                   account_id;
      fc::optional<fc::variant>         public_data;
      fc::optional<public_key_type>     active_key;
      uint8_t                           delegate_pay_rate = -1;

      bool is_retracted()const;
      bool is_delegate()const;

      void evaluate( transaction_evaluation_state& eval_state )const;
      void evaluate_v1( transaction_evaluation_state& eval_state )const;
   };

   struct update_signing_key_operation
   {
      static const operation_type_enum  type;

      account_id_type  account_id;
      public_key_type  signing_key;

      void evaluate( transaction_evaluation_state& eval_state )const;
   };

   struct withdraw_pay_operation
   {
      static const operation_type_enum type;
      withdraw_pay_operation():amount(0){}

      withdraw_pay_operation( share_type amount_to_withdraw,
                              account_id_type id )
      :amount(amount_to_withdraw),account_id(id) {}

      share_type                       amount;
      account_id_type                  account_id;

      void evaluate( transaction_evaluation_state& eval_state )const;
   };

} } // bts::blockchain

FC_REFLECT( bts::blockchain::register_account_operation, (name)(public_data)(owner_key)(active_key)(delegate_pay_rate)(meta_data) )
FC_REFLECT( bts::blockchain::update_account_operation, (account_id)(public_data)(active_key)(delegate_pay_rate) )
FC_REFLECT( bts::blockchain::withdraw_pay_operation, (amount)(account_id) )
FC_REFLECT( bts::blockchain::update_signing_key_operation, (account_id)(signing_key) )
