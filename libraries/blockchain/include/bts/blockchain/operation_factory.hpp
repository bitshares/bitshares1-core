#pragma once
#include <bts/blockchain/operations.hpp>

namespace bts { namespace blockchain {

   /**
    * @class operation_factory
    *
    *  Enables polymorphic creation and serialization of operation objects in
    *  an manner that can be extended by derived chains.
    */
   class operation_factory
   {
       public:
          static operation_factory& instance();
          class operation_converter_base
          {
             public:
                  virtual ~operation_converter_base(){};
                  virtual void to_variant( const bts::blockchain::operation& in, fc::variant& out ) = 0;
                  virtual void from_variant( const fc::variant& in, bts::blockchain::operation& out ) = 0;
          };

          template<typename OperationType>
          class operation_converter : public operation_converter_base
          {
             public:
                  virtual void to_variant( const bts::blockchain::operation& in, fc::variant& output )
                  { try {
                     FC_ASSERT( in.type == OperationType::type );
                     fc::mutable_variant_object obj( "type", in.type );

                     obj[ "data" ] = fc::raw::unpack<OperationType>(in.data);

                     output = std::move(obj);
                  } FC_RETHROW_EXCEPTIONS( warn, "" ) }
                  virtual void from_variant( const fc::variant& in, bts::blockchain::operation& output )
                  { try {
                     auto obj = in.get_object();

                     FC_ASSERT( output.type == OperationType::type );
                     output.data = fc::raw::pack( obj["data"].as<OperationType>() );
                  } FC_RETHROW_EXCEPTIONS( warn, "type: ${type}", ("type",fc::get_typename<OperationType>::name()) ) }
          };

          template<typename OperationType>
          void   register_operation()
          {
            _converters[OperationType::type] = std::make_shared< operation_converter<OperationType> >(); 
          }

          /// defined in operations.cpp
          void to_variant( const bts::blockchain::operation& in, fc::variant& output );
          /// defined in operations.cpp
          void from_variant( const fc::variant& in, bts::blockchain::operation& output );
       private: 

          std::unordered_map<int, std::shared_ptr<operation_converter_base> > _converters;

   };

} } // bts::blockchain 
