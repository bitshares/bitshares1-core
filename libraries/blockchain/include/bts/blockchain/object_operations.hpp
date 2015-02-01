#pragma once
#include <bts/blockchain/operations.hpp>
#include <bts/blockchain/object_record.hpp>

namespace bts { namespace blockchain {

   /**
    *  Defines or updates an object.
    */
   struct set_object_operation
   {
      static const operation_type_enum type;

      set_object_operation(){}
      set_object_operation( const object_record& obj )
          :obj(obj)
      {}

      object_record obj;

      void evaluate( transaction_evaluation_state& eval_state )const;
   };

}} // bts::blockchain

FC_REFLECT( bts::blockchain::set_object_operation, (obj) );
