#include <bts/dns/dns_wallet.hpp>

namespace bts { namespace dns {

using namespace bts::blockchain;

dns_wallet::dns_wallet()
{
}

dns_wallet::~dns_wallet()
{
}

signed_transaction dns_wallet::bid_on_domain(const std::string &name, asset amount, dns_db &db)
{ try {
    // Check inputs
    FC_ASSERT(is_valid_name(name), "Invalid name");
    FC_ASSERT(is_valid_amount(amount), "Invalid amount");

    // Name should be new, for auction, or expired
    signed_transactions txs = signed_transactions(); // TODO: use current tx pool
    bool new_or_expired;
    output_reference prev_tx_ref;
    FC_ASSERT(name_is_available(name, txs, db, new_or_expired, prev_tx_ref), "Name not available");

    signed_transaction trx;
    auto total_in = asset(); // set by collect_inputs
    auto req_sigs = std::unordered_set<address>();
    auto change_addr = new_recv_address("Change address");

    // Init output
    auto domain_output = claim_domain_output();
    domain_output.name = name;
    domain_output.value = std::vector<char>();
    domain_output.owner = new_recv_address("Owner address for domain: " + name);
    domain_output.state = claim_domain_output::possibly_in_auction;

    if (new_or_expired)
    {
        trx.inputs = collect_inputs(amount, total_in, req_sigs);
        auto change_amt = total_in - amount;

        trx.outputs.push_back(trx_output(domain_output, amount));
        trx.outputs.push_back(trx_output(claim_by_signature_output(change_addr), change_amt));
    }
    else
    {
        auto prev_output = get_tx_ref_output(prev_tx_ref, db);
        auto prev_dns_output = to_dns_output(prev_output);

        auto old_ask_amt = prev_output.amount.get_rounded_amount();
        auto required_in = DNS_MIN_BID_FROM(old_ask_amt);
        FC_ASSERT(amount >= required_in, "Minimum bid amount not met");

        trx.inputs = collect_inputs(required_in, total_in, req_sigs);
        auto change_amt = total_in - required_in;
        
        // TODO: macro-ize (also lift from validator)
        auto amt_to_past_owner = old_ask_amt + ((required_in - old_ask_amt) / 2);

        // fee is implicit from difference
        // auto amt_as_fee = (required_in - old_ask_amt) / 2;

        trx.outputs.push_back(trx_output(claim_by_signature_output(prev_dns_output.owner), amt_to_past_owner));
        trx.outputs.push_back(trx_output(domain_output, amount));
        trx.outputs.push_back(trx_output(claim_by_signature_output(change_addr), change_amt));
    }

    // Sign
    trx.sigs.clear();
    sign_transaction(trx, req_sigs, false); // TODO: what is last arg
    trx = add_fee_and_sign(trx, amount, total_in, req_sigs);

    return trx;

} FC_RETHROW_EXCEPTIONS(warn, "bid_on_domain ${name} with ${amt}", ("name", name)("amt", amount)) }

signed_transaction dns_wallet::update_domain_record(const std::string &name, fc::variant value, dns_db &db)
{ try {
    // Check inputs
    FC_ASSERT(is_valid_name(name), "Invalid name");
    FC_ASSERT(is_valid_value(value), "Invalid value");

    // Build dns output
    auto output = claim_domain_output();
    output.name = name;
    output.value = serialize_value(value);
    output.state = claim_domain_output::not_in_auction;

    return update_or_auction_domain(true, output, asset(), db);

} FC_RETHROW_EXCEPTIONS(warn, "update_domain_record ${name} with value ${val}", ("name", name)("val", value)) }

signed_transaction dns_wallet::auction_domain(const std::string &name, asset amount, dns_db &db)
{ try {
    // Check inputs
    FC_ASSERT(is_valid_name(name), "Invalid name");
    FC_ASSERT(is_valid_amount(amount), "Invalid amount");

    // Build dns output
    auto output = claim_domain_output();
    output.name = name;
    output.value = std::vector<char>();
    output.owner = new_recv_address("Domain sale address");
    output.state = claim_domain_output::possibly_in_auction;

    return update_or_auction_domain(false, output, amount, db);

} FC_RETHROW_EXCEPTIONS(warn, "auction_domain ${name} with ${amt}", ("name", name)("amt", amount)) }

signed_transaction dns_wallet::update_or_auction_domain(bool update, claim_domain_output &output, asset amount,
                                                        dns_db &db)
{ try {
    // Check inputs
    FC_ASSERT(is_valid_amount(amount), "Invalid amount");

    // Name should exist and be owned
    signed_transactions txs = signed_transactions(); // TODO: use current tx pool
    output_reference prev_tx_ref;
    FC_ASSERT(name_is_useable(output.name, txs, db, get_unspent_outputs(), prev_tx_ref), "Name unavailable for update or auction");

    // Build inputs
    signed_transaction trx;
    auto total_in = asset(); // set by collect_inputs
    auto req_sigs = std::unordered_set<address>();
    trx.inputs = collect_inputs(asset(), total_in, req_sigs);
    trx.inputs.push_back(trx_input(prev_tx_ref));

    // Build outputs
    if (update)
        output.owner = to_dns_output(get_tx_ref_output(prev_tx_ref, db)).owner;

    auto change_addr = new_recv_address("Change address");
    auto change_amt = total_in;
    trx.outputs.push_back(trx_output(output, amount));
    trx.outputs.push_back(trx_output(claim_by_signature_output(change_addr), change_amt));

    // Sign
    trx.sigs.clear();
    sign_transaction(trx, req_sigs, false); // TODO: what is last arg
    trx = add_fee_and_sign(trx, asset(), total_in, req_sigs);

    return trx;

} FC_RETHROW_EXCEPTIONS(warn, "update_or_auction_domain ${name} with ${amt}", ("name", output.name)("amt", amount)) }

bool dns_wallet::scan_output(transaction_state& state, const trx_output& out, const output_reference& ref,
                             const output_index& idx)
{
    try {
        switch ( out.claim_func )
        {
            case claim_domain:
            {
                if (is_my_address( out.as<claim_domain_output>().owner ))
                {
                    cache_output( out, ref, idx );
                    return true;
                }
                return false;
            }
            default:
                return wallet::scan_output( state, out, ref, idx );
        }
    } FC_RETHROW_EXCEPTIONS( warn, "" )
}

/* Warning! Assumes last output is the change address! */
// TODO you know what happens when you assume...
signed_transaction dns_wallet::add_fee_and_sign(signed_transaction &trx, asset required_in, asset& total_in,
                                                std::unordered_set<address> req_sigs)
{
    uint64_t trx_bytes = fc::raw::pack( trx ).size();
    bts::blockchain::asset fee = get_fee_rate() * trx_bytes;
    auto change = trx.outputs.back().amount;
    auto change_addr = trx.outputs.back().as<claim_by_signature_output>().owner;

    wlog("req in: ${req}, total in: ${tot}, fee:${fee}", ("req", required_in)("tot", total_in)("fee", fee));
    if (total_in >= required_in + fee)
    {
        change = change - fee;
        trx.outputs.back() = trx_output( claim_by_signature_output( change_addr ), change );
        if ( change == asset() )
        {
            trx.outputs.pop_back();
        }
    }
    else 
    {
        fee = fee + fee; //TODO should be recursive since you could grab extra from lots of inputs
        req_sigs.clear();
        total_in = asset();
        trx.inputs = collect_inputs( required_in + fee, total_in, req_sigs);
        change = total_in - (required_in + fee);
        trx.outputs.back() = trx_output( claim_by_signature_output( change_addr ), change );
        if ( change == asset() )
        {
            trx.outputs.pop_back();
        }

    }
    trx.sigs.clear();
    sign_transaction( trx, req_sigs, true );
    return trx;
}

}} // bts::dns
