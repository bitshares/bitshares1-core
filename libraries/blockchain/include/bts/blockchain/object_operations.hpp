#pragma once
#include <bts/blockchain/operations.hpp>
#include <bts/blockchain/object_operations.hpp>
#include <bts/blockchain/object_record.hpp>

namespace bts { namespace blockchain {

   /**
    *  Defines or updates an object.
    */
   struct set_object_operation
   {
      static const operation_type_enum type;

      set_object_operation(){}
      set_object_operation( int64_t id, const object_record& o )
          :id(id),
          obj( o )
      {}

      // This is not the real ID with type info - object record should have a type
      // If ID is zero, make a new object (get a new ID)
      // if ID is negative, look in evaluation stack
      // if ID is positive, update the existing object
      int64_t id = 0;
      object_record obj;

      void evaluate( transaction_evaluation_state& eval_state );
   };

}} // bts::blockchain

FC_REFLECT( bts::blockchain::set_object_operation, (id)(obj) );
