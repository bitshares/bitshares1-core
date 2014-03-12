#pragma once

namespace bts { namespace btsx {
   using namespace bts::blockchain;
   
   class btsx_evaluation_state : public transaction_evaluation_state
   {
        public:
           btsx_evaluation_state( const signed_transaction& trx )
           :transaction_evaluation_state( trx ){}
   };

   class btsx_transaction_validator : public bts::blockchain::transaction_validator
   {
       public:
          virtual void evaluate( const signed_transaction& trx );
          virtual void validate_input( const meta_trx_input& in, transaction_evaluation_state& state );
          virtual void validate_output( const trx_output& out, transaction_evaluation_state& state );
   };

} } // bts::btsx
