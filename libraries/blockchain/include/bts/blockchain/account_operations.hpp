#pragma once
#include <bts/blockchain/operations.hpp>
#include <bts/blockchain/extended_address.hpp>

namespace bts { namespace blockchain { 

   struct register_account_operation
   {
      static const operation_type_enum type; 
      register_account_operation():is_delegate(false){}

      register_account_operation( const std::string& name, 
                              const fc::variant& json_data, 
                              const public_key_type& owner, 
                              const public_key_type& active, 
                              bool  as_delegate = false );
      
      std::string         name;
      fc::variant         json_data;
      public_key_type     owner_key;
      public_key_type     active_key;
      bool                is_delegate;
   };

   struct update_account_operation
   {
      static const operation_type_enum type; 

      update_account_operation():account_id(0),is_delegate(false){}

      /** this should be 0 for creating a new name */
      account_id_type                   account_id;
      fc::optional<fc::variant>         json_data;
      fc::optional<public_key_type>     active_key;
      bool                              is_delegate;
   };

   struct withdraw_pay_operation
   {
      static const operation_type_enum type; 
      withdraw_pay_operation():amount(0){}

      share_type                       amount;
      account_id_type                  account_id;
   };

} } // bts::blockchain

FC_REFLECT( bts::blockchain::register_account_operation, (name)(json_data)(owner_key)(active_key)(is_delegate) )
FC_REFLECT( bts::blockchain::update_account_operation, (account_id)(json_data)(active_key)(is_delegate) )
FC_REFLECT( bts::blockchain::withdraw_pay_operation, (amount)(account_id) )
