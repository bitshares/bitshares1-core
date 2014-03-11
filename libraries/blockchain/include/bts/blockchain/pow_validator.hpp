#pragma once


namespace bts { namespace blockchain {

   class pow_validator
   {
      public:
        virtual ~pow_validator(){}
        bool    validate_work( const block_header& header ) { return true; }
   };

   typedef std::shared_ptr<pow_validator> pow_validator_ptr;

} } // bts::blockchain
