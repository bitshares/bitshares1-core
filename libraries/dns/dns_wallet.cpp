#include <bts/dns/dns_wallet.hpp>

namespace bts { namespace dns {

dns_wallet::dns_wallet()
{
}

dns_wallet::~dns_wallet()
{
}

signed_transaction dns_wallet::bid_on_domain(const std::string &name, const asset &bid_price,
                                             const signed_transactions &tx_pool, dns_db &db)
{ try {
    FC_ASSERT(is_valid_name(name), "Invalid name");

    /* Name should be new, for auction, or expired */
    bool new_or_expired;
    output_reference prev_tx_ref;
    FC_ASSERT(name_is_available(name, tx_pool, db, new_or_expired, prev_tx_ref), "Name not available");

    /* Build domain output */
    auto domain_output = claim_domain_output();
    domain_output.name = name;
    domain_output.value = std::vector<char>();
    domain_output.owner = new_recv_address("Owner address for domain: " + name);
    domain_output.last_tx_type = claim_domain_output::bid_or_auction;

    signed_transaction tx;
    std::unordered_set<address> req_sigs;

    if (new_or_expired)
    {
        tx.outputs.push_back(trx_output(domain_output, bid_price));
    }
    else
    {
        tx.inputs.push_back(trx_input(prev_tx_ref));

        auto prev_output = get_tx_ref_output(prev_tx_ref, db);
        asset transfer_amount;
        FC_ASSERT(is_valid_bid_price(prev_output.amount, bid_price, transfer_amount), "Invalid bid price");

        /* Fee is implicit from difference */
        auto prev_dns_output = to_dns_output(prev_output);
        tx.outputs.push_back(trx_output(claim_by_signature_output(prev_dns_output.owner), transfer_amount));
        tx.outputs.push_back(trx_output(domain_output, bid_price));
    }

    return collect_inputs_and_sign(tx, bid_price, req_sigs);

} FC_RETHROW_EXCEPTIONS(warn, "bid_on_domain ${name} with ${amt}", ("name", name)("amt", bid_price)) }

signed_transaction dns_wallet::update_domain_record(const std::string &name, const fc::variant &value,
                                                    const signed_transactions &tx_pool, dns_db &db)
{ try {
    FC_ASSERT(is_valid_name(name), "Invalid name");
    FC_ASSERT(is_valid_value(value), "Invalid value");

    /* Build domain output */
    auto domain_output = claim_domain_output();
    domain_output.name = name;
    domain_output.value = serialize_value(value);
    domain_output.last_tx_type = claim_domain_output::update;

    return update_or_auction_domain(true, domain_output, asset(), tx_pool, db);

} FC_RETHROW_EXCEPTIONS(warn, "update_domain_record ${name} with value ${val}", ("name", name)("val", value)) }

signed_transaction dns_wallet::auction_domain(const std::string &name, const asset &ask_price,
                                              const signed_transactions &tx_pool, dns_db &db)
{ try {
    FC_ASSERT(is_valid_name(name), "Invalid name");

    /* Build domain output */
    auto domain_output = claim_domain_output();
    domain_output.name = name;
    domain_output.value = std::vector<char>();
    domain_output.last_tx_type = claim_domain_output::bid_or_auction;

    return update_or_auction_domain(false, domain_output, ask_price, tx_pool, db);

} FC_RETHROW_EXCEPTIONS(warn, "auction_domain ${name} with ${amt}", ("name", name)("amt", ask_price)) }

signed_transaction dns_wallet::update_or_auction_domain(bool update, claim_domain_output &domain_output,
                                                        asset amount, const signed_transactions &tx_pool,
                                                        dns_db &db)
{ try {
    /* Name should exist and be owned */
    output_reference prev_tx_ref;
    FC_ASSERT(name_is_useable(domain_output.name, tx_pool, db, get_unspent_outputs(), prev_tx_ref),
            "Name unavailable for update or auction");
    auto prev_output = get_tx_ref_output(prev_tx_ref, db);

    auto prev_domain_output = to_dns_output(prev_output);
    domain_output.owner = prev_domain_output.owner;

    if (update)
        amount = prev_output.amount;

    signed_transaction tx;
    std::unordered_set<address> req_sigs;

    tx.inputs.push_back(trx_input(prev_tx_ref));
    tx.outputs.push_back(trx_output(domain_output, amount));
    req_sigs.insert(prev_domain_output.owner);

    return collect_inputs_and_sign(tx, asset(), req_sigs);

} FC_RETHROW_EXCEPTIONS(warn, "update_or_auction_domain ${name} with ${amt}", ("name", domain_output.name)("amt", amount)) }

// TODO: Put this in the parent wallet class
signed_transaction dns_wallet::collect_inputs_and_sign(signed_transaction &trx, const asset &min_amnt,
                                                       std::unordered_set<address> &req_sigs, const address &change_addr)
{
    /* Save transaction inputs and outputs */
    std::vector<trx_input> original_inputs = trx.inputs;
    std::vector<trx_output> original_outputs = trx.outputs;
    std::unordered_set<address> original_req_sigs = req_sigs;

    asset required_in = min_amnt;
    asset total_in;

    do
    {
        /* Restore original transaction */
        trx.inputs = original_inputs;
        trx.outputs = original_outputs;
        req_sigs = original_req_sigs;
        total_in = 0u;

        auto new_inputs = collect_inputs(required_in, total_in, req_sigs); /* Throws if insufficient funds */
        trx.inputs.insert(trx.inputs.end(), new_inputs.begin(), new_inputs.end());

        auto change_amt = total_in - required_in;
        trx.outputs.push_back(trx_output(claim_by_signature_output(change_addr), change_amt));

        trx.sigs.clear();
        sign_transaction(trx, req_sigs, false);

        /* Calculate fee and subtract from change */
        auto fee = get_fee_rate() * trx.size();
        required_in += fee;
        change_amt = total_in - required_in;

        if (change_amt > 0u)
            trx.outputs.back() = trx_output(claim_by_signature_output(change_addr), change_amt);
        else
            trx.outputs.pop_back();
    }
    while (total_in < required_in); /* Try again if the fee ended up too high for the collected inputs */

    trx.sigs.clear();
    sign_transaction(trx, req_sigs, true);

    return trx;
}

signed_transaction dns_wallet::collect_inputs_and_sign(signed_transaction &trx, const asset &min_amnt,
                                                       std::unordered_set<address> &req_sigs)
{
    return collect_inputs_and_sign(trx, min_amnt, req_sigs, new_recv_address("Change address"));
}

bool dns_wallet::scan_output(transaction_state &state, const trx_output &out, const output_reference &ref,
                             const output_index &idx)
{
    if (!is_dns_output(out))
        return wallet::scan_output(state, out, ref, idx);

    auto domain_output = to_dns_output(out);

    if (is_my_address(domain_output.owner))
    {
        cache_output(out, ref, idx);
        return true;
    }

    return false;
}

} } // bts::dns
