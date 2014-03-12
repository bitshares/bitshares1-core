#include <bts/dns/dns_transaction_validator.hpp>
#include <bts/dns/outputs.hpp>

namespace bts { namespace dns {

transaction_summary dns_transaction_validator::evaluate( const signed_transaction& tx)
{
    dns_tx_evaluation_state state(tx);
    return on_evaluate( state );
}


void dns_transaction_validator::validate_input( const meta_trx_input& in, transaction_evaluation_state& state )
{
     switch( in.output.claim_func )
     {
        case claim_domain:
            validate_domain_input(in, state);
           break;
        default:
           transaction_validator::validate_input( in, state );
     }
}

void dns_transaction_validator::validate_output( const trx_output& out, transaction_evaluation_state& state )
{
     switch( out.claim_func )
     {
        case claim_domain:
            validate_domain_output(out, state);
           break;
        default:
           transaction_validator::validate_output( out, state );
     }
}

void dns_transaction_validator::validate_domain_input(const meta_trx_input& in, transaction_evaluation_state& state)
{

}

void dns_transaction_validator::validate_domain_output(const trx_output& out, transaction_evaluation_state& state)
{
    auto dns_state = dynamic_cast<dns_tx_evaluation_state&>(state);
    //FC_ASSERT( dns_state != NULL, "Error casting tx_evaluation_state to dns_tx_evaluation_state");
    FC_ASSERT( ! dns_state.seen_domain_claim,
            "More than one domain claim output in one tx: ${tx}", ("tx", state.trx) );
    dns_state.seen_domain_claim = true;
}


}} // bts::blockchain
