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
      set_object_operation( const object_record& o )
      :obj( std::move(obj) ){}

      int64_t id; // if not zero, overrides "number" field of ID with either this or id with this index
      object_record obj;

      void evaluate( transaction_evaluation_state& eval_state );
   };

}} // bts::blockchain

FC_REFLECT( bts::blockchain::set_object_operation, (id)(obj) );
