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
    auto total_in = asset();
    auto req_sigs = std::unordered_set<address>();
    tx.inputs = collect_inputs(bid_price, total_in, req_sigs);

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

    auto change_addr = new_recv_address("Change address");
    auto change_amt = total_in - bid_price;
    tx.outputs.push_back(trx_output(claim_by_signature_output(change_addr), change_amt));

    tx.sigs.clear();
    sign_transaction(tx, req_sigs, false);

    return add_fee_and_sign(tx, bid_price, total_in, req_sigs);

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
    auto total_in = asset();
    auto req_sigs = std::unordered_set<address>();
    req_sigs.insert(prev_domain_output.owner);
    tx.inputs = collect_inputs(asset(), total_in, req_sigs);

    tx.inputs.push_back(trx_input(prev_tx_ref));
    tx.outputs.push_back(trx_output(domain_output, amount));

    auto change_addr = new_recv_address("Change address");
    auto change_amt = total_in;
    tx.outputs.push_back(trx_output(claim_by_signature_output(change_addr), change_amt));

    tx.sigs.clear();
    sign_transaction(tx, req_sigs, false);

    return add_fee_and_sign(tx, asset(), total_in, req_sigs);

} FC_RETHROW_EXCEPTIONS(warn, "update_or_auction_domain ${name} with ${amt}",
        ("name", domain_output.name)("amt", amount)) }

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

} } // bts::dns
