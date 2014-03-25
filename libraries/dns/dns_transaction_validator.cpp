#include <bts/dns/dns_transaction_validator.hpp>
#include <bts/dns/dns_util.hpp>

namespace bts { namespace dns {

dns_transaction_validator::dns_transaction_validator(dns_db* db)
:transaction_validator(db)
{
    
}

dns_transaction_validator::~dns_transaction_validator()
{
}

transaction_summary dns_transaction_validator::evaluate( const signed_transaction& tx, 
                                                         const block_evaluation_state_ptr& block_state )
{
    dns_tx_evaluation_state state(tx);
    return on_evaluate( state, block_state );
}


void dns_transaction_validator::validate_input( const meta_trx_input& in, 
                                                transaction_evaluation_state& state,
                                                const block_evaluation_state_ptr& block_state)
{
     switch( in.output.claim_func )
     {
        case claim_domain:
            validate_domain_input(in, state, block_state);
            break;

        default:
            transaction_validator::validate_input( in, state, block_state );
            break;
     }
}

void dns_transaction_validator::validate_output( const trx_output& out, transaction_evaluation_state& state, const block_evaluation_state_ptr& block_state )
{
     switch( out.claim_func )
     {
        case claim_domain:
            validate_domain_output( out, state, block_state );
            break;

        default:
            transaction_validator::validate_output( out, state, block_state );
            break;
     }
}

// 
void dns_transaction_validator::validate_domain_input(const meta_trx_input& in, transaction_evaluation_state& state, const block_evaluation_state_ptr& block_state)
{
    //TODO a lot of the input validation logic is in the output validator right now
    auto dns_state = dynamic_cast<dns_tx_evaluation_state&>(state);

    FC_ASSERT(!dns_state.seen_domain_input,
              "More than one domain claim intput in one tx: ${tx}", ("tx", state.trx));

    dns_state.seen_domain_input = true;
    dns_state.prev_output = in.output;
    dns_state.prev_dns_output = to_dns_output(dns_state.prev_output);
}

void dns_transaction_validator::validate_domain_output(const trx_output& out, transaction_evaluation_state& state, const block_evaluation_state_ptr& block_state)
{
    ilog("Validating domain claim output");

    //TODO assert "amount" doesn't change when updating domain record so
    //that domains can contribute to "valid votes" ??

    // Check inputs
    FC_ASSERT(is_dns_output(out), "Invalid output");
    auto dns_out = to_dns_output(out);
    FC_ASSERT(is_valid_name(dns_out.name), "Invalid name");
    FC_ASSERT(is_valid_value(dns_out.value), "Invalid value");

    auto dns_state = dynamic_cast<dns_tx_evaluation_state&>(state);
    FC_ASSERT(!dns_state.seen_domain_output,
              "More than one domain claim output in one tx: ${tx}", ("tx", state.trx) );
    dns_state.seen_domain_output = true;

    dns_db* db = dynamic_cast<dns_db*>(_db);
    FC_ASSERT( db != nullptr );

    // Check if valid bid
    // TODO: Should use tx pool from block evaluation state
    signed_transactions empty_txs = signed_transactions();
    bool name_exists;
    auto prev_output = trx_output();
    uint32_t prev_output_age;
    auto valid_bid = is_valid_bid(out, empty_txs, *db, name_exists, prev_output, prev_output_age);

    /* If we haven't seen a domain input then the only valid output is a new
     * domain auction. */
    if (!dns_state.seen_domain_input)
    {
        ilog("Have not seen a domain claim input on this tx");
        FC_ASSERT(!name_exists, "Name already exists (and is younger than 1 block-year)"); 
        FC_ASSERT(valid_bid, "Invalid buy tx on new or expired name");
        return;
    }

    ilog("Seen a domain input");

    //TODO do this just from the input without looking into the DB?
    /* Otherwise, the transaction must have a domain input and it must exist
     * in the database, and it can't be expired */
    FC_ASSERT(name_exists, "Name doesn't exist");
    FC_ASSERT(!is_expired_age(prev_output_age), "Name is expired");
    FC_ASSERT(dns_out.name == dns_state.prev_dns_output.name, "Bid tx refers to different input and output names");
   
    // case on state of claimed output
    //   * if auction is over (not_for_sale OR output is older than 3 days)
    if (dns_out.state == claim_domain_output::not_in_auction || !is_auction_age(prev_output_age))
    {
        ilog("Auction is over.");

        // If you're the owner, do whatever you like!
        FC_ASSERT(state.has_signature(dns_out.owner),
                  "Domain tx requiring signature doesn't have it: ${tx}", ("tx", state.trx));

        ilog("Tx signed by owner");
    }
    else
    {
        // Currently in an auction
        ilog("Currently in an auction");

        FC_ASSERT(dns_out.state == claim_domain_output::possibly_in_auction,
                  "Bid made without keeping for_auction flag");

        //TODO use macros in dns_config.hpp instead of hard-coded constants
        //TODO restore
        FC_ASSERT(out.amount.get_rounded_amount() >= 
                  (11 * dns_state.prev_output.amount.get_rounded_amount()) / 10,
                  "Bid was too small: ${trx}", ("trx", state.trx) );

        // Half of difference goes to fee
        dns_state.add_required_fees((out.amount - dns_state.prev_output.amount) / 2);

        // Check for output to past owner
        bool found = false;
        for (auto other_out : state.trx.outputs)
        {
            bool right_claim = other_out.claim_func == claim_by_signature;
            bool enough = (other_out.amount == 
                  dns_state.prev_output.amount + (out.amount - dns_state.prev_output.amount / 2));
            bool to_owner = right_claim && 
                 other_out.as<claim_by_signature_output>().owner == dns_state.prev_dns_output.owner;

            if (right_claim && enough && to_owner) 
            {
                found = true;
                break;
            }
        }

        FC_ASSERT(found, "Bid did not pay enough to previous owner");
    }
}

}} // bts::blockchain
