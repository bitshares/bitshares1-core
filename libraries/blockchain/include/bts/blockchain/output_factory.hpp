#pragma once
#include <bts/blockchain/transaction.hpp>

namespace bts { namespace blockchain {

   /**
    * @class output_factory
    *
    *  Enables polymorphic creation and serialization of output objects in
    *  an manner that can be extended by derived chains.
    */
   class output_factory
   {
       public:
          static output_factory& instance();
          class claim_converter_base
          {
             public:
                  virtual ~claim_converter_base(){};
                  virtual void to_variant( const trx_output& in, fc::variant& out ) = 0;
                  virtual void from_variant( const fc::variant& in, trx_output& output ) = 0;
          };

          template<typename OutputType>
          class claim_converter : public claim_converter_base
          {
             public:
                  virtual void to_variant( const trx_output& in, fc::variant& output )
                  {
                     FC_ASSERT( in.claim_func == OutputType::type );
                     fc::mutable_variant_object obj;

                     obj["amount"]     = in.amount; 
                     obj["claim_func"] = in.claim_func;
                     obj["claim_data"] = fc::raw::unpack<OutputType>(in.claim_data);

                     output = std::move(obj);
                  }
                  virtual void from_variant( const fc::variant& in, trx_output& output )
                  {
                     auto obj = in.get_object();
                     output.amount = obj["amount"].as<asset>();

                     FC_ASSERT( output.claim_func == OutputType::type );
                     output.claim_data = fc::raw::pack( obj["claim_data"].as<OutputType>() );
                  }
          };

          template<typename OutputType>
          void   register_output()
          {
            _converters[OutputType::type] = std::make_shared< claim_converter<OutputType> >(); 
          }

          /// defined in transaction.cpp
          void to_variant( const trx_output& in, fc::variant& output );
          /// defined in transaction.cpp
          void from_variant( const fc::variant& in, trx_output& output );
       private: 

          std::unordered_map<int, std::shared_ptr<claim_converter_base> > _converters;

   };

} } // bts::blockchain 
