#include <bts/btsx/btsx_transaction_validator.hpp>
#include <bts/btsx/outputs.hpp>
#include <fc/reflect/variant.hpp>


namespace bts { namespace btsx {


    transaction_summary btsx_transaction_validator::evaluate( const signed_transaction& trx )
    { try {
       btsx_evaluation_state state(trx);
       return on_evaluate( state );
    } FC_RETHROW_EXCEPTIONS( warn, "", ("trx",trx) ) }

    void btsx_transaction_validator::validate_input( const meta_trx_input& in, transaction_evaluation_state& state )
    { try {
         switch( in.output.claim_func )
         {
            case claim_by_bid:
               validate_bid_input( in, state );            
               break;
            case claim_by_long:
               validate_long_input( in, state );            
               break;
            case claim_by_cover:
               validate_cover_input( in, state );            
               break;
            default:
               transaction_validator::validate_input( in, state );
         }
    } FC_RETHROW_EXCEPTIONS( warn, "" ) }

    void btsx_transaction_validator::validate_output( const trx_output& out, transaction_evaluation_state& state )
    { try {
         switch( out.claim_func )
         {
            case claim_by_bid:
               validate_bid_output( out, state );            
               break;
            case claim_by_long:
               validate_long_output( out, state );            
               break;
            case claim_by_cover:
               validate_cover_output( out, state );            
               break;
            default:
               transaction_validator::validate_output( out, state );
         }
    } FC_RETHROW_EXCEPTIONS( warn, "" ) }

    void btsx_transaction_validator::validate_bid_output( const trx_output& out, transaction_evaluation_state& state )
    { try {

    } FC_RETHROW_EXCEPTIONS( warn, "" ) }

    void btsx_transaction_validator::validate_bid_input( const meta_trx_input& in, transaction_evaluation_state& state )
    { try {

    } FC_RETHROW_EXCEPTIONS( warn, "" ) }
   
    void btsx_transaction_validator::validate_long_output( const trx_output& out, transaction_evaluation_state& state )
    { try {

    } FC_RETHROW_EXCEPTIONS( warn, "" ) }

    void btsx_transaction_validator::validate_long_input( const meta_trx_input& in, transaction_evaluation_state& state )
    { try {

    } FC_RETHROW_EXCEPTIONS( warn, "" ) }
   
    void btsx_transaction_validator::validate_cover_output( const trx_output& out, transaction_evaluation_state& state )
    { try {

    } FC_RETHROW_EXCEPTIONS( warn, "" ) }

    void btsx_transaction_validator::validate_cover_input( const meta_trx_input& in, transaction_evaluation_state& state )
    { try {

    } FC_RETHROW_EXCEPTIONS( warn, "" ) }

} } // bts::btsx
