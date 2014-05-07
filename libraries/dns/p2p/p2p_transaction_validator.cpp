#include <bts/dns/p2p/p2p_transaction_validator.hpp>

namespace bts { namespace dns { namespace p2p {

p2p_transaction_validator::p2p_transaction_validator(dns_db* db) : bts::dns::dns_transaction_validator(db)
{
}

p2p_transaction_validator::~p2p_transaction_validator()
{
}

block_evaluation_state_ptr p2p_transaction_validator::create_block_state() const
{
    return std::make_shared<p2p_block_evaluation_state>(_dns_db);
}

transaction_summary p2p_transaction_validator::evaluate(const signed_transaction& tx,
                                                        const block_evaluation_state_ptr& block_state)
{ try {
    p2p_transaction_evaluation_state state(tx);

    return on_evaluate(state, block_state);
} FC_RETHROW_EXCEPTIONS( warn, "" ) }

void p2p_transaction_validator::validate_input(const meta_trx_input& in, transaction_evaluation_state& state,
                                               const block_evaluation_state_ptr& block_state)
{ try {
    if (is_dns_output(in.output))
    {
        claim_dns_output dns_input = to_dns_output(in.output);
        p2p_transaction_evaluation_state& dns_state = dynamic_cast<p2p_transaction_evaluation_state&>(state);
        const p2p_block_evaluation_state_ptr dns_block_state = std::dynamic_pointer_cast<p2p_block_evaluation_state>(block_state);
        FC_ASSERT(dns_block_state);

        validate_p2p_input(dns_input, in.output.amount, dns_state, dns_block_state);
    }
    else
    {
        transaction_validator::validate_input(in, state, block_state);
    }
} FC_RETHROW_EXCEPTIONS( warn, "" ) }

void p2p_transaction_validator::validate_output(const trx_output& out, transaction_evaluation_state& state,
                                                const block_evaluation_state_ptr& block_state)
{ try {
    if (is_dns_output(out))
    {
        claim_dns_output dns_output = to_dns_output(out);
        p2p_transaction_evaluation_state& dns_state = dynamic_cast<p2p_transaction_evaluation_state&>(state);
        const p2p_block_evaluation_state_ptr dns_block_state = std::dynamic_pointer_cast<p2p_block_evaluation_state>(block_state);
        FC_ASSERT(dns_block_state);

        validate_p2p_output(dns_output, out.amount, dns_state, dns_block_state);

        /* Add key to pending keys list */
        dns_block_state->pending_keys.push_back(dns_output.key);
    }
    else
    {
        transaction_validator::validate_output(out, state, block_state);
    }
} FC_RETHROW_EXCEPTIONS( warn, "" ) }

bool p2p_transaction_validator::is_valid_output(const claim_dns_output& output)
{
    return (is_valid_key(output.key) && is_valid_value(output.value));
}

bool p2p_transaction_validator::is_valid_key(const std::string& key)
{
    return key.size() <= DNS_MAX_KEY_LEN;
}

bool p2p_transaction_validator::is_valid_value(const std::vector<char>& value)
{
    return value.size() <= DNS_MAX_VALUE_LEN;
}

bool p2p_transaction_validator::is_valid_bid_price(const asset& bid_price, const asset& prev_bid_price)
{
    if (bid_price < asset(DNS_MIN_BID_FROM(prev_bid_price.amount), prev_bid_price.unit))
        return false;

    return true;
}

asset p2p_transaction_validator::get_bid_transfer_amount(const asset& bid_price, const asset& prev_bid_price)
{
    FC_ASSERT(is_valid_bid_price(bid_price, prev_bid_price), "Invalid bid price");

    return bid_price - DNS_BID_FEE_RATIO(bid_price - prev_bid_price);
}

bool p2p_transaction_validator::auction_is_closed(const output_reference& output_ref)
{
    auto output = _dns_db->fetch_output(output_ref);
    FC_ASSERT(is_dns_output(output));

    auto dns_output = to_dns_output(output);
    auto age = _dns_db->get_output_age(output_ref);

    if (dns_output.last_tx_type == claim_dns_output::last_tx_type_enum::auction
        && age < DNS_AUCTION_DURATION_BLOCKS)
        return false;

    return true;
}

bool p2p_transaction_validator::key_is_expired(const output_reference& output_ref)
{
    auto output = _dns_db->fetch_output(output_ref);
    FC_ASSERT(is_dns_output(output));

    if (!auction_is_closed(output_ref))
        return false;

    auto dns_output = to_dns_output(output);
    auto age = _dns_db->get_output_age(output_ref);

    if (dns_output.last_tx_type == claim_dns_output::last_tx_type_enum::auction)
        return age >= (DNS_AUCTION_DURATION_BLOCKS + DNS_EXPIRE_DURATION_BLOCKS);

    FC_ASSERT(dns_output.last_tx_type == claim_dns_output::last_tx_type_enum::update);

    return age >= DNS_EXPIRE_DURATION_BLOCKS;
}

/* Check if key is available for bid: new, in auction, or expired */
bool p2p_transaction_validator::key_is_available(const std::string& key, const std::vector<std::string>& pending_keys,
                                                 bool& new_or_expired, output_reference& prev_output_ref)
{
    new_or_expired = false;

    if (std::find(pending_keys.begin(), pending_keys.end(), key) != pending_keys.end())
        return false;

    if (!_dns_db->has_dns_ref(key))
    {
        new_or_expired = true;
        return true;
    }

    prev_output_ref = _dns_db->get_dns_ref(key);

    if (key_is_expired(prev_output_ref))
        new_or_expired = true;

    return !auction_is_closed(prev_output_ref) || new_or_expired;
}

/* Check if key is available for value update or auction */
bool p2p_transaction_validator::key_is_useable(const std::string& key, const std::vector<std::string>& pending_keys,
                                               const std::vector<std::string>& unspent_keys, output_reference& prev_output_ref)
{
    if (std::find(pending_keys.begin(), pending_keys.end(), key) != pending_keys.end())
        return false;

    if (!_dns_db->has_dns_ref(key))
        return false;

    prev_output_ref = _dns_db->get_dns_ref(key);

    if (!auction_is_closed(prev_output_ref))
        return false;

    if (key_is_expired(prev_output_ref))
        return false;

    /* Check if spendable */
    if (std::find(unspent_keys.begin(), unspent_keys.end(), key) == unspent_keys.end())
        return false;

    return true;
}

void p2p_transaction_validator::validate_p2p_input(const claim_dns_output& input, const asset& amount,
                                                   p2p_transaction_evaluation_state& state,
                                                   const p2p_block_evaluation_state_ptr& block_state)
{
    ilog("Validating dns claim input");
    FC_ASSERT(!state.seen_dns_input, "More than one dns claim input in tx: ${tx}", ("tx", state.trx));
    FC_ASSERT(_dns_db->has_dns_ref(input.key), "Input references invalid key");

    auto output_ref = _dns_db->get_dns_ref(input.key);
    FC_ASSERT(!key_is_expired(output_ref), "Input key is expired");

    if (auction_is_closed(output_ref))
        FC_ASSERT(state.has_signature(input.owner), "Non-bid input not signed by owner");

    state.add_input_asset(amount);
    state.dns_input = input;
    state.dns_input_amount = amount;
    state.seen_dns_input = true;
}

void p2p_transaction_validator::validate_p2p_output(const claim_dns_output& output, const asset& amount,
                                                    p2p_transaction_evaluation_state& state,
                                                    const p2p_block_evaluation_state_ptr& block_state)
{
    ilog("Validating dns claim output");
    FC_ASSERT(!state.seen_dns_output, "More than one dns claim output in tx: ${tx}", ("tx", state.trx));
    FC_ASSERT(is_valid_output(output), "Invalid DNS output");

    state.add_output_asset(amount);
    state.seen_dns_output = true;

    /* Check key status */
    bool new_or_expired;
    output_reference prev_output_ref;
    auto available = key_is_available(output.key, block_state->pending_keys, new_or_expired, prev_output_ref);

    /* If we haven't seen a dns input then the only valid output is a new dns auction */
    if (!state.seen_dns_input)
    {
        ilog("Have not seen a dns claim input on this tx");
        FC_ASSERT(new_or_expired && available, "Key already exists (and is younger than 1 block-year)");
        return;
    }

    /* Otherwise, the transaction must have a dns input and it must exist in the database,
     * and it can't be expired */
    ilog("Seen a dns input");
    FC_ASSERT(!new_or_expired, "Key new or expired");
    FC_ASSERT(output.key == state.dns_input.key, "Bid tx refers to different input and output keys");

    /* Bid in existing auction */
    if (!auction_is_closed(prev_output_ref))
    {
        ilog("Currently in an auction");
        FC_ASSERT(available, "Key not available");
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
    FC_ASSERT(!key_is_expired(prev_output_ref), "Key is expired");

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
    auto prev_dns_output = to_dns_output(_dns_db->fetch_output(prev_output_ref));
    FC_ASSERT(state.has_signature(prev_dns_output.owner), "DNS tx missing required signature: ${tx}", ("tx", state.trx));
    ilog("Tx signed by owner");
}

} } } // bts::dns::p2p
