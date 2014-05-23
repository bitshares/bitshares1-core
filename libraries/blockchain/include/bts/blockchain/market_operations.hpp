#pragma once
#include <bts/blockchain/operations.hpp>

namespace bts { namespace blockchain {

   struct bid_operation
   {
        static const operation_type_enum type; 

        share_type   amount;
        address      owner;
        name_id_type delegate_id;
   };

   struct ask_operation
   {
        static const operation_type_enum type; 

        share_type   amount;
        address      owner;
        name_id_type delegate_id;
   };

   struct short_operation
   {
        static const operation_type_enum type; 

        share_type   amount;
        address      owner;
        name_id_type delegate_id;
   };
   
   struct cover_operation
   {
        static const operation_type_enum type; 

        share_type   amount;
        address      owner;
        name_id_type delegate_id;
   };

   struct add_collateral_operation
   {
        static const operation_type_enum type; 

        share_type   amount;
        address      owner;
        name_id_type delegate_id;
   };

   struct remove_collateral_operation
   {
        static const operation_type_enum type; 

        share_type   amount;
        address      owner;
        name_id_type delegate_id;
   };

} } // bts::blockchain
