#pragma once
#include <bts/blockchain/transaction_validator.hpp>

namespace bts { namespace btsx {
   using namespace bts::blockchain;
   class btsx_db;
   
   class btsx_evaluation_state : public transaction_evaluation_state
   {
        public:
           btsx_evaluation_state( const signed_transaction& trx )
           :transaction_evaluation_state( trx ){}
   };

   class btsx_transaction_validator : public bts::blockchain::transaction_validator
   {
       public:
          btsx_transaction_validator( btsx_db* db );

          virtual transaction_summary evaluate( const signed_transaction& trx );

          virtual void validate_input( const meta_trx_input& in, transaction_evaluation_state& state );
          virtual void validate_output( const trx_output& out, transaction_evaluation_state& state );

          virtual void validate_bid_output( const trx_output& out, transaction_evaluation_state& state );
          virtual void validate_bid_input( const meta_trx_input& in, transaction_evaluation_state& state );

          virtual void validate_long_output( const trx_output& out, transaction_evaluation_state& state );
          virtual void validate_long_input( const meta_trx_input& in, transaction_evaluation_state& state );

          virtual void validate_cover_output( const trx_output& out, transaction_evaluation_state& state );
          virtual void validate_cover_input( const meta_trx_input& in, transaction_evaluation_state& state );
   };

} } // bts::btsx
