#include <bts/dns/dns_transaction_validator.hpp>
#include <bts/dns/outputs.hpp>
#include <bts/dns/dns_db.hpp>
#include <bts/blockchain/config.hpp>

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
    auto dns_state = dynamic_cast<dns_tx_evaluation_state&>(state);
    FC_ASSERT( ! dns_state.seen_domain_input,
                "More than one domain claim intput in one tx: ${tx}", ("tx", state.trx) );
    dns_state.seen_domain_input = true;
    dns_state.claimed = in.output;
    dns_state.dns_claimed = in.output.as<claim_domain_output>();
}

void dns_transaction_validator::validate_domain_output(const trx_output& out, transaction_evaluation_state& state)
{
    auto dns_state = dynamic_cast<dns_tx_evaluation_state&>(state);
    FC_ASSERT( ! dns_state.seen_domain_output,
            "More than one domain claim output in one tx: ${tx}", ("tx", state.trx) );
    dns_state.seen_domain_output = true;

    // "name" and "value" length limits
    auto dns_out = out.as<claim_domain_output>();

    dns_db* db = dynamic_cast<dns_db*>(_db);
    FC_ASSERT( db != nullptr );


    /* If we haven't seen a domain input then the only valid output is a new
     * domain auction.
     */
    if (! dns_state.seen_domain_input)
    {
        // name doesn't already exist  (not in DB OR older than 1 year)
        if ( db->has_dns_record(dns_out.name) )
        {
            auto old_record = db->get_dns_record(dns_out.name);
            auto old_tx_id = old_record.last_update_ref.trx_hash;
            auto block_num = db->fetch_trx_num(old_tx_id).block_num;
            auto current_block = db->head_block_num();

            if (current_block - block_num < BTS_BLOCKCHAIN_BLOCKS_PER_YEAR)
            {
                FC_ASSERT(!"Name already exists (and is younger than 1 block-year)"); 
            }
        }
        FC_ASSERT(dns_out.flags == claim_domain_output::for_auction,
                  "New auction started with for_auction flag not set");
        return;
    }


    /* Otherwise, the transaction must have a domain input and it must exist
     * in the database, and it can't be expired
     */
    //TODO do this just from the input without looking into the DB?
    FC_ASSERT( db->has_dns_record(dns_out.name),
               "Transaction references a name that doesn't exist");
    auto old_record = db->get_dns_record(dns_out.name);
    auto old_tx_id = old_record.last_update_ref.trx_hash;
    auto block_num = db->fetch_trx_num(old_tx_id).block_num;
    auto current_block = db->head_block_num();
    auto block_age = current_block - block_num;
    FC_ASSERT( block_age < BTS_BLOCKCHAIN_BLOCKS_PER_YEAR,
             "Domain transaction references an expired domain as an input");
        
    FC_ASSERT(dns_out.name == dns_state.dns_claimed.name,
              "bid transaction refers to different input and output names");

    
    // case on state of claimed output
    //   * if auction is over (not_for_sale OR output is older than 3 days)
    if (dns_out.flags == claim_domain_output::not_for_sale
       || block_age >= 3 * BTS_BLOCKCHAIN_BLOCKS_PER_DAY) 
    {
        // If you're the owner, do whatever you like!
        if (! state.has_signature(dns_out.owner) )
        {
            FC_ASSERT(false, "Domain tx requiring signature doesn't have it: ${tx}",
                     ("tx", state.trx));
        }

    } else {
        // Currently in an auction
        FC_ASSERT(dns_out.flags == claim_domain_output::for_auction,
                  "bid made without keeping for_auction flag");
        //TODO use macros in dns_config.hpp instead of hard-coded constants
        //TODO restore
        FC_ASSERT(out.amount.get_rounded_amount() >= 
                  (11 * dns_state.claimed.amount.get_rounded_amount()) / 10,
                  "Bid was too small: ${trx}", ("trx", state.trx) );
        // half of difference goes to fee
        dns_state.add_required_fees((out.amount - dns_state.claimed.amount) / 2);

        // check for output to past owner
        bool found = false;
        for (auto other_out : state.trx.outputs)
        {
            bool right_claim = other_out.claim_func == claim_by_signature;
            bool enough = (other_out.amount == 
                  dns_state.claimed.amount + (out.amount - dns_state.claimed.amount / 2));
            bool to_owner = right_claim && 
                 other_out.as<claim_by_signature_output>().owner == dns_state.dns_claimed.owner;
            if (right_claim && enough && to_owner) 
            {
                found = true;
                break;
            }
        }
        if (!found) 
        {
            FC_ASSERT(!"Bid did not pay enough to previous owner");
        }
    }
}


}} // bts::blockchain
