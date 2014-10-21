#pragma once
#include <fc/reflect/reflect.hpp>
#include <fc/exception/exception.hpp>
#include <fc/io/enum_type.hpp>
#include <vector>

namespace bts { namespace vm {
   using std::vector;
   using fc::variant;
   using fc::variant_object;

   class engine 
   {
       public:
          enum op_code
          {
             ADD,
             SUB,
             MULT,
             DIV,
             PUSH,
             LT,
             GT,
             LTEQ,
             GTEQ,
             EQ,
             NEQ,
             NOT_OP,
             POP,
             PUSH_CHILD,
             SET_CHILD,
             PUSH_INDEX,
             SET_INDEX,
             SET,
             PUSH_SIZE
          };

          struct operation
          {
             fc::enum_type<uint16_t,op_code> code;
             int16_t                         arg1;
             int16_t                         arg2;
             int16_t                         arg3;
             fc::variant                     arg0;
          };

          void execute( const vector<operation>& ops );

          const variant&      get_value( const variant& op_value, uint16_t stack_index )
          {
             return stack_index ? stack[stack.size() - stack_index] : op_value;
          }

          fc::variant_object& get_object( uint16_t stack_index )
          {
             FC_ASSERT( stack_index != 0 );
             return stack[stack.size()-stack_index].get_object();
          }

          const fc::variant_object& get_object( const variant& op_value, uint16_t stack_index )
          {
             return stack_index ? stack[stack.size()-stack_index].get_object() : op_value.get_object();
          }
          const std::string& get_string( const variant& op_value, uint16_t stack_index )
          {
             return stack_index ? stack[stack.size()-stack_index].get_string() : op_value.get_string();
          }

       private:
          fc::variants stack;
   };

} }

FC_REFLECT_ENUM( bts::vm::engine::op_code, (ADD)(SUB)(MULT)(DIV)(PUSH)(LT)(GT)(LTEQ)(GTEQ)(NOT_OP)(POP) )
FC_REFLECT( bts::vm::engine::operation, (code)(arg1)(arg2)(arg0) )
