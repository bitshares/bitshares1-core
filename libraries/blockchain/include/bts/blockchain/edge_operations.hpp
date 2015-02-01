#pragma once
#include <bts/blockchain/operations.hpp>
#include <bts/blockchain/edge_record.hpp>
#include <fc/io/enum_type.hpp>
#include <fc/io/raw.hpp>
#include <fc/io/raw_fwd.hpp>
#include <fc/reflect/reflect.hpp>

namespace bts { namespace blockchain {

   /**
    *  Defines or updates an edge.
    */
   struct set_edge_operation
   {
      static const operation_type_enum type;

      set_edge_operation(){}
      set_edge_operation( const edge_record& edge )
          :edge(object_record( edge, edge_object, 0 ))
      {}

      object_record edge;

      void evaluate( transaction_evaluation_state& eval_state )const;
   };

}} // bts::blockchain

FC_REFLECT( bts::blockchain::set_edge_operation, (edge) )
