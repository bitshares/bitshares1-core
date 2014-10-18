#include <bts/vm/engine.hpp>

namespace bts { namespace vm {
   void engine::execute( const vector<operation>& ops )
   {
       uint32_t pcount = 0;
       if( ops.size() == 0 ) return;


       while( pcount < ops.size() )
       {
          const operation& op = ops[pcount];
          switch( (op_code)op.code )
          {
             case PUSH:
                stack.emplace_back( get_value( op.arg0, op.arg1 ) );
                break;
             case SET:
                FC_ASSERT( op.arg1 != 0 );
                stack[ stack.size() - op.arg1 ] = get_value( op.arg0, op.arg2 );
                break;
             case POP:
                stack.pop_back();
                break;
             case ADD:
                stack.back() = stack.back() + get_value( op.arg0, op.arg1 ); 
                break;
             case MULT:
                stack.back() = stack.back() * get_value( op.arg0, op.arg1 ); 
                break;
             case SUB:
                stack.back() = stack.back() - get_value( op.arg0, op.arg1 ); 
                break;
             case DIV:
                stack.back() = stack.back() / get_value( op.arg0, op.arg1 ); 
                break;
             case LT:
                stack.back() = stack.back() < get_value( op.arg0, op.arg1 ); 
                break;
             case GT:
                stack.back() = stack.back() > get_value( op.arg0, op.arg1 ); 
                break;
             case LTEQ:
                stack.back() = stack.back() <= get_value( op.arg0, op.arg1 ); 
                break;
             case GTEQ:
                stack.back() = stack.back() >= get_value( op.arg0, op.arg1 ); 
                break;
             case EQ:
                stack.back() = stack.back() == get_value( op.arg0, op.arg1 ); 
                break;
             case NEQ:
                stack.back() = !(stack.back() == get_value( op.arg0, op.arg1 ) ); 
                break;
             case NOT_OP:
                stack.back() = !get_value( op.arg0, op.arg1 ).as_bool();
                break;
             case PUSH_CHILD: 
                // read object from arg1 location, read child name from arg2 location and store a copy of the object at the end of the stack.
                stack.emplace_back( get_object( op.arg0, op.arg1 )[ get_string( op.arg0, op.arg2 )] );
                break;
             case SET_CHILD: 
             {
                fc::mutable_variant_object mutable_obj( get_object( op.arg1 ) );
                mutable_obj[ get_string( op.arg0, op.arg2 )] = get_value( op.arg0, op.arg3 );
                get_object( op.arg1 ) = std::move(mutable_obj);
             }
             break;
             case PUSH_INDEX: 
                break;
             case SET_INDEX: 
                break;
             case PUSH_SIZE: // push number of items in array or object located at arg1
                stack.emplace_back( variant(get_value( op.arg0, op.arg1 ).size()) );
                break;
          };
          ++pcount;
       }
   }
}} 
