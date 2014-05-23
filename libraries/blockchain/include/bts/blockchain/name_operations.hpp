#pragma once
#include <bts/blockchain/operations.hpp>

namespace bts { namespace blockchain { 

   struct reserve_name_operation
   {
      static const operation_type_enum type; 
      reserve_name_operation():is_delegate(false){}

      reserve_name_operation( const std::string& name, 
                              const fc::variant& json_data, 
                              const public_key_type& owner, 
                              const public_key_type& active, 
                              bool as_delegate = false );
      
      std::string         name;
      fc::variant         json_data;
      public_key_type     owner_key;
      public_key_type     active_key;
      bool                is_delegate;
   };

   struct update_name_operation
   {
      static const operation_type_enum type; 

      update_name_operation():name_id(0),is_delegate(false){}

      /** this should be 0 for creating a new name */
      name_id_type                  name_id;
      fc::optional<fc::variant>     json_data;
      fc::optional<public_key_type> active_key;
      bool                          is_delegate;
   };

} } // bts::blockchain

FC_REFLECT( bts::blockchain::reserve_name_operation, (name)(json_data)(owner_key)(active_key)(is_delegate) )
FC_REFLECT( bts::blockchain::update_name_operation, (name_id)(json_data)(active_key)(is_delegate) )
