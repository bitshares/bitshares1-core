#include <bts/dns/dns_transaction_validator.hpp>
#include <bts/dns/util.hpp>

namespace bts { namespace dns {

dns_transaction_validator::dns_transaction_validator(dns_db *db) : transaction_validator(db)
{
    _dns_db = dynamic_cast<dns_db *>(db);
    FC_ASSERT(_dns_db != nullptr);
}

dns_transaction_validator::~dns_transaction_validator()
{
}

block_evaluation_state_ptr dns_transaction_validator::create_block_state() const
{
    return std::make_shared<dns_block_evaluation_state>();
}

transaction_summary dns_transaction_validator::evaluate(const signed_transaction &tx,
                                                        const block_evaluation_state_ptr &block_state)
{
    dns_tx_evaluation_state state(tx);

    return on_evaluate(state, block_state);
}

void dns_transaction_validator::validate_input(const meta_trx_input &in, transaction_evaluation_state &state,
                                               const block_evaluation_state_ptr &block_state)
{
    if (is_dns_output(in.output))
    {
        claim_dns_output dns_input = to_dns_output(in.output);
        dns_tx_evaluation_state &dns_state = dynamic_cast<dns_tx_evaluation_state &>(state);
        const dns_block_evaluation_state_ptr dns_block_state = std::dynamic_pointer_cast<dns_block_evaluation_state>(block_state);
        FC_ASSERT(dns_block_state);

        validate_dns_input(dns_input, in.output.amount, dns_state, dns_block_state);
    }
    else
    {
        transaction_validator::validate_input(in, state, block_state);
    }
}

void dns_transaction_validator::validate_output(const trx_output &out, transaction_evaluation_state &state,
                                                const block_evaluation_state_ptr &block_state)
{
    if (is_dns_output(out))
    {
        claim_dns_output dns_output = to_dns_output(out);
        dns_tx_evaluation_state &dns_state = dynamic_cast<dns_tx_evaluation_state &>(state);
        const dns_block_evaluation_state_ptr dns_block_state = std::dynamic_pointer_cast<dns_block_evaluation_state>(block_state);
        FC_ASSERT(dns_block_state);

        validate_dns_output(dns_output, out.amount, dns_state, dns_block_state);

        /* Add key to key pool */
        dns_block_state->key_pool.push_back(dns_output.key);
    }
    else
    {
        transaction_validator::validate_output(out, state, block_state);
    }
}

void dns_transaction_validator::validate_dns_input(const claim_dns_output &input, const asset &amount,
                                                      dns_tx_evaluation_state &state,
                                                      const dns_block_evaluation_state_ptr &block_state)
{
    ilog("Validating dns claim input");
    FC_ASSERT(!state.seen_dns_input, "More than one dns claim input in tx: ${tx}", ("tx", state.trx));

    FC_ASSERT(_dns_db->has_dns_ref(input.key), "Input references invalid key");
    // TODO: make sure input is not expired and has signature

    state.add_input_asset(amount);
    state.dns_input = input;
    state.dns_input_amount = amount;
    state.seen_dns_input = true;
}

void dns_transaction_validator::validate_dns_output(const claim_dns_output &output, const asset &amount,
                                                       dns_tx_evaluation_state &state,
                                                       const dns_block_evaluation_state_ptr &block_state)
{
    ilog("Validating dns claim output");
    FC_ASSERT(!state.seen_dns_output, "More than one dns claim output in tx: ${tx}", ("tx", state.trx));
    FC_ASSERT(output.is_valid(), "Invalid DNS output");

    state.seen_dns_output = true;
    state.add_output_asset(amount);

    /* Check key status */
    bool new_or_expired;
    output_reference prev_tx_ref;
    auto available = key_is_available(output.key, block_state->key_pool, *_dns_db, new_or_expired, prev_tx_ref);

    /* If we haven't seen a dns input then the only valid output is a new dns auction */
    if (!state.seen_dns_input)
    {
        ilog("Have not seen a dns claim input on this tx");
        FC_ASSERT(new_or_expired && available, "key already exists (and is younger than 1 block-year)");
        return;
    }

    /* Otherwise, the transaction must have a dns input and it must exist in the database,
     * and it can't be expired */
    ilog("Seen a dns input");
    FC_ASSERT(!new_or_expired, "key new or expired");
    FC_ASSERT(output.key == state.dns_input.key, "Bid tx refers to different input and output keys");

    /* Bid in existing auction */
    if (!auction_is_closed(prev_tx_ref, *_dns_db))
    {
        ilog("Currently in an auction");
        FC_ASSERT(available, "key not available");
        FC_ASSERT(state.dns_input.last_tx_type == claim_dns_output::last_tx_type_enum::auction, "Input not for auction");

        FC_ASSERT(is_valid_bid_price(amount, state.dns_input_amount), "Invalid bid amount");
        auto amount_back = get_bid_transfer_amount(amount, state.dns_input_amount);
        state.add_required_fees(amount - amount_back);

        /* Check for output to past owner */
        bool found = false;
        for (auto other_out : state.trx.outputs)
        {
            bool right_claim = other_out.claim_func == claim_by_signature;
            bool enough = other_out.amount >= amount_back;
            bool to_owner = right_claim
                && other_out.as<claim_by_signature_output>().owner == state.dns_input.owner;

            if (right_claim && enough && to_owner)
            {
                found = true;
                break;
            }
        }

        FC_ASSERT(found, "Bid did not pay enough to previous owner");

        return;
    }

    /* Update or sale */
    ilog("Auction is over.");
    FC_ASSERT(!key_is_expired(prev_tx_ref, *_dns_db), "key is expired");

    /* Keep output amount constant when updating dns record */
    if (output.last_tx_type == claim_dns_output::last_tx_type_enum::update)
    {
        FC_ASSERT(amount == state.dns_input_amount, "Output amount should not change when updating record");
    }
    else
    {
        FC_ASSERT(output.last_tx_type == claim_dns_output::last_tx_type_enum::auction, "Invalid last_tx_type");
    }

    /* If you're the owner, do whatever you like! */
    auto prev_dns_output = to_dns_output(get_tx_ref_output(prev_tx_ref, *_dns_db));
    FC_ASSERT(state.has_signature(prev_dns_output.owner), "dns tx missing required signature: ${tx}", ("tx", state.trx));
    ilog("Tx signed by owner");
}

} } // bts::dns
