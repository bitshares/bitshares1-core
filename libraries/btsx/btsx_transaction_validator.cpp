#include <bts/btsx/btsx_transaction_validator.hpp>


namespace bts { namespace btsx {


    void btsx_transaction_validator::evaluate( const signed_transaction& trx )
    {
       btsx_evaluation_state state(trx);
       on_evaluate( state );
    }

    void btsx_transaction_validator::validate_input( const meta_trx_input& in, transaction_evaluation_state& state )
    {
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
    }

    void btsx_transaction_validator::validate_output( const trx_output& out, transaction_evaluation_state& state )
    {
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
    }

} } // bts::btsx
