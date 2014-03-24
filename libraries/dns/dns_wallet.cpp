#include <bts/dns/dns_wallet.hpp>

namespace bts { namespace dns {

using namespace bts::blockchain;

dns_wallet::dns_wallet()
{
}

dns_wallet::~dns_wallet()
{
}

signed_transaction dns_wallet::buy_domain(const std::string& name, asset amount, dns_db& db)
{ try {
    // Check inputs
    FC_ASSERT(is_valid_name(name), "Invalid name");
    FC_ASSERT(is_valid_amount(amount), "Invalid amount");

    signed_transaction trx;
   
    auto domain_addr = new_recv_address("Owner address for domain: " + name);
    auto change_addr = new_recv_address("Change address");
    auto req_sigs = std::unordered_set<bts::blockchain::address>();
    auto inputs = std::vector<trx_input>();
    auto total_in = bts::blockchain::asset(); // set by collect_inputs

    // Name should be new or already in an auction
    signed_transactions empty_txs; // TODO: pass current txs (block eval state)
    bool name_exists;
    auto prev_output = trx_output();
    uint32_t prev_output_age;
    FC_ASSERT(can_bid_on_name(name, empty_txs, db, name_exists, prev_output, prev_output_age), "Name not available");

    // Init output
    auto domain_output = claim_domain_output();
    domain_output.name = name;
    domain_output.value = std::vector<char>();
    domain_output.owner = domain_addr;
    domain_output.state = claim_domain_output::possibly_in_auction;

    if (!name_exists)
    {
        trx.inputs = collect_inputs( amount, total_in, req_sigs );
        auto change_amt = total_in - amount;

        trx.outputs.push_back( trx_output( domain_output, amount ) );
        trx.outputs.push_back( trx_output( claim_by_signature_output( change_addr ), change_amt ) );
    }
    else
    {
        auto prev_dns_output = prev_output.as<claim_domain_output>();
        auto old_ask_amt = prev_output.amount.get_rounded_amount();
        auto required_in = DNS_MIN_BID_FROM(old_ask_amt);
        FC_ASSERT(amount >= required_in, "Minimum bid amount not met");

        trx.inputs = collect_inputs( required_in, total_in, req_sigs);
        auto change_amt = total_in - required_in;
        
        //TODO macro-ize
        auto amt_to_past_owner = old_ask_amt + ((required_in - old_ask_amt) / 2);

        // fee is implicit from difference
        //auto amt_as_fee = (required_in - old_ask_amt) / 2;

        trx.outputs.push_back( trx_output( claim_by_signature_output( prev_dns_output.owner ),
                                           amt_to_past_owner ) );
        trx.outputs.push_back( trx_output( domain_output, amount ) );
        trx.outputs.push_back( trx_output( claim_by_signature_output( change_addr ), change_amt ) );
    }

    trx.sigs.clear();
    sign_transaction(trx, req_sigs, false); //TODO what is last arg?

    trx = add_fee_and_sign(trx, amount, total_in, req_sigs);

    return trx;

} FC_RETHROW_EXCEPTIONS(warn, "buy_domain ${name} with ${amt}", ("name", name)("amt", amount)) }

signed_transaction dns_wallet::update_record(const std::string& name, fc::variant value, dns_db& db)
{ try {
    // Check inputs
    FC_ASSERT(is_valid_name(name), "Invalid name");
    auto serialized_value = serialize_value(value);
    FC_ASSERT(is_valid_value(value), "Invalid value");

    // Check expiry
    auto record = db.get_dns_record(name);
    auto utxo_ref = record.last_update_ref;
    auto block_num = db.fetch_trx_num(utxo_ref.trx_hash).block_num;
    auto current_block = db.head_block_num();

    FC_ASSERT(current_block - block_num < DNS_EXPIRE_DURATION_BLOCKS, "Tried to update expired record");
   
    signed_transaction trx;
    auto change_addr = new_recv_address("Change address");
    auto req_sigs = std::unordered_set<bts::blockchain::address>();
    bts::blockchain::asset total_in; // set by collect_inputs
    trx.inputs = collect_inputs( asset(), total_in, req_sigs );
   
    address domain_addr;
    auto found = false;
    for (auto pair : get_unspent_outputs())
    {
        if (pair.second.claim_func == claim_domain)
        {
            auto dns_output = pair.second.as<claim_domain_output>();

            if (dns_output.name != name)
                continue;

            // TODO: The record needs to be initially set not_for_sale before the first update_record
            //FC_ASSERT(dns_output.flags == claim_domain_output::not_for_sale,
                    //"Tried to update record that is still for sale");

            domain_addr = dns_output.owner;
            trx.inputs.push_back( trx_input( get_ref_from_output_idx(pair.first) ) );

            found = true;
            break;
        }
    } 

    FC_ASSERT(found, "Tried to update record but name UTXO not found in wallet");

    auto change_amt = total_in;

    auto domain_output = claim_domain_output();
    domain_output.name = name;
    domain_output.value = serialized_value;
    domain_output.owner = domain_addr;
    domain_output.state = claim_domain_output::not_in_auction;

    trx.outputs.push_back( trx_output( domain_output, asset() ) );
    trx.outputs.push_back( trx_output( claim_by_signature_output( change_addr ), change_amt ) );

    trx.sigs.clear();
    sign_transaction(trx, req_sigs, false);

    trx = add_fee_and_sign(trx, asset(), total_in, req_sigs);

    return trx;
} FC_RETHROW_EXCEPTIONS(warn, "update_record ${name} with value ${val}", ("name", name)("val", value)) }

signed_transaction dns_wallet::sell_domain(const std::string& name, asset amount, dns_db& db)
{ try {
    // Check inputs
    FC_ASSERT(is_valid_name(name), "Invalid name");
    FC_ASSERT(is_valid_amount(amount), "Invalid amount");

    // Check expiry
    auto record = db.get_dns_record(name);
    auto utxo_ref = record.last_update_ref;
    auto block_num = db.fetch_trx_num(utxo_ref.trx_hash).block_num;
    auto current_block = db.head_block_num();

    FC_ASSERT(current_block - block_num < DNS_EXPIRE_DURATION_BLOCKS, "Tried to sell expired domain");

    signed_transaction trx;
   
    auto change_addr = new_recv_address("Change address");
    auto sale_addr = new_recv_address("Domain sale address");
    auto req_sigs = std::unordered_set<bts::blockchain::address>();
    bts::blockchain::asset total_in; // set by collect_inputs
   
    trx.inputs = collect_inputs( asset(), total_in, req_sigs );
    auto found = false;
    for (auto pair : get_unspent_outputs())
    {
        if (pair.second.claim_func == claim_domain)
        {
            auto dns_output = pair.second.as<claim_domain_output>();

            if (dns_output.name != name)
                continue;

            // TODO: The record needs to be initially set not_for_sale before the first update_record
            //FC_ASSERT(dns_output.flags == claim_domain_output::not_for_sale,
                    //"Tried to update record that is still for sale");

            trx.inputs.push_back( trx_input( get_ref_from_output_idx(pair.first) ) );

            found = true;
            break;
        }
    } 

    FC_ASSERT(found, "Tried to update record but name UTXO not found in wallet");

    auto change_amt = total_in;

    auto domain_output = claim_domain_output();
    domain_output.name = name;
    domain_output.value = std::vector<char>();
    domain_output.owner = sale_addr;
    domain_output.state = claim_domain_output::possibly_in_auction;

    trx.outputs.push_back( trx_output( domain_output, amount ) );
    trx.outputs.push_back( trx_output( claim_by_signature_output( change_addr ), change_amt ) );

    trx.sigs.clear();
    sign_transaction(trx, req_sigs, false);

    trx = add_fee_and_sign(trx, asset(), total_in, req_sigs);

    return trx;
} FC_RETHROW_EXCEPTIONS(warn, "sell_domain ${name} with ${amt}", ("name", name)("amt", amount)) }

bool dns_wallet::scan_output( const trx_output& out,
                              const output_reference& ref,
                              const bts::wallet::output_index& oidx )
{
    try {
        switch ( out.claim_func )
        {
            case claim_domain:
            {
                if (is_my_address( out.as<claim_domain_output>().owner ))
                {
                    cache_output( out, ref, oidx );
                    return true;
                }
                return false;
            }
            default:
                return wallet::scan_output( out, ref, oidx );
        }
    } FC_RETHROW_EXCEPTIONS( warn, "" )
}

/* Warning! Assumes last output is the change address! */
// TODO you know what happens when you assume...
signed_transaction dns_wallet::add_fee_and_sign(signed_transaction& trx,
                                                bts::blockchain::asset required_in,
                                                bts::blockchain::asset& total_in,
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
