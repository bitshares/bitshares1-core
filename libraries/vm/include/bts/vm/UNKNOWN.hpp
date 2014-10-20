#pragma once
#include <fc/reflect/reflect.hpp>
#include <fc/io/enum_type.hpp>
#include <vector>

namespace bts { namespace vm {
   using std::vector;

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
          };

          struct operation
          {
             fc::enum_type<uint16_t,op_code> code;
             int16_t                         arg1;
             int16_t                         arg2;
             fc::variant                     arg0;
          };

          void execute( const vector<operation>& ops );

       private:
          fc::variants stack;
   };

} }

FC_REFLECT_ENUM( bts::vm::engine::op_code, (ADD)(SUB)(MULT)(DIV)(PUSH)(LT)(GT)(LTEQ)(GTEQ)(NOT_OP)(POP) )
FC_REFLECT( bts::vm::engine::operation, (code)(arg1)(arg2)(arg0) )
