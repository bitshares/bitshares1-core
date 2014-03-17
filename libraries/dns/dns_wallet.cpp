#include <bts/blockchain/config.hpp>
#include <bts/wallet/wallet.hpp>

#include <bts/dns/dns_wallet.hpp>
#include <bts/dns/outputs.hpp>
#include <bts/dns/dns_config.hpp>

#include<fc/reflect/variant.hpp>
#include<fc/io/raw.hpp>
#include<fc/io/raw_variant.hpp>

#include <fc/log/logger.hpp>

namespace bts { namespace dns {

using namespace bts::blockchain;

dns_wallet::dns_wallet()
//:my(new detail::dns_wallet_impl())
{
}

dns_wallet::~dns_wallet()
{
}

bts::blockchain::signed_transaction dns_wallet::buy_domain(
                    const std::string& name, bts::blockchain::asset amount, dns_db& db)
{ try {
    signed_transaction trx;
    // TODO check name length
   
    auto domain_addr = new_recv_address("Owner address for domain: " + name);
    auto change_addr = new_recv_address("Change address");
    auto req_sigs = std::unordered_set<bts::blockchain::address>();
    auto inputs = std::vector<trx_input>();
    auto total_in = bts::blockchain::asset(); // set by collect_inputs

    // if there is record and it's not expired and it's for sale, make a bid
    if (db.has_dns_record(name))
    {
        // check "for sale" state
        auto old_record = db.get_dns_record(name);
        auto old_utxo_ref = old_record.last_update_ref;
        auto old_output = db.fetch_output(old_utxo_ref);
        auto old_dns_output = old_output.as<claim_domain_output>();
        if (old_dns_output.flags != claim_domain_output::for_auction)
        {
            FC_ASSERT(!"tried to make a bid for a name that is not for sale");
        }
        auto block_num = db.fetch_trx_num(old_utxo_ref.trx_hash).block_num;
        auto current_block = db.head_block_num();
        // check age
        // TODO put constant 3 in config
        if (current_block - block_num < BTS_BLOCKCHAIN_BLOCKS_PER_DAY * 3)
        {

            auto domain_output = claim_domain_output();
            domain_output.name = name;
            domain_output.value = std::vector<char>();
            domain_output.owner = domain_addr;
            domain_output.flags = claim_domain_output::for_auction;

            auto old_ask_amt = old_output.amount.get_rounded_amount();
            auto required_in = BTS_DNS_MIN_BID_FROM(old_ask_amt);
            trx.inputs = collect_inputs( required_in, total_in, req_sigs);
            auto change_amt = total_in - required_in;
            
            //TODO macro-ize
            auto amt_to_past_owner = old_ask_amt + ((required_in - old_ask_amt) / 2);

            // fee is implicit from difference
            //auto amt_as_fee = (required_in - old_ask_amt) / 2;

            trx.outputs.push_back( trx_output( claim_by_signature_output( old_dns_output.owner ),
                                               amt_to_past_owner ) );
            trx.outputs.push_back( trx_output( domain_output, amount ) );
            trx.outputs.push_back( trx_output( claim_by_signature_output( change_addr ), change_amt ) );

            trx.sigs.clear();
            sign_transaction(trx, req_sigs, false);

            trx = add_fee_and_sign(trx, amount, total_in, req_sigs);

            return trx;

        } else if (current_block - block_num < BTS_BLOCKCHAIN_BLOCKS_PER_YEAR) {
            FC_ASSERT(!"Tried to bid on domain that is not for sale (auction timed out but not explicitly marked as done)");
        }
    }
    
    // otherwise, you're starting a new auction
    
    trx.inputs = collect_inputs( amount, total_in, req_sigs );
    auto change_amt = total_in - amount;

    auto domain_output = claim_domain_output();
    domain_output.name = name;
    domain_output.value = std::vector<char>();
    domain_output.owner = domain_addr;
    domain_output.flags = claim_domain_output::for_auction;

    trx.outputs.push_back( trx_output( domain_output, amount ) );
    trx.outputs.push_back( trx_output( claim_by_signature_output( change_addr ), change_amt ) );

    trx.sigs.clear();
    sign_transaction(trx, req_sigs, false); //TODO what is last arg?

    trx = add_fee_and_sign(trx, amount, total_in, req_sigs);

    return trx;

} FC_RETHROW_EXCEPTIONS(warn, "buy_domain ${name} with ${amt}", ("name", name)("amt", amount)) }

bts::blockchain::signed_transaction dns_wallet::update_record(
                                             const std::string& name, address domain_addr,
                                             fc::variant value)
{ try {
    signed_transaction trx;
     // TODO check name length
   
    auto change_addr = new_recv_address("Change address");
    auto req_sigs = std::unordered_set<bts::blockchain::address>();
    auto inputs = std::vector<trx_input>();
    bts::blockchain::asset total_in; // set by collect_inputs

   
    auto serialized_value = fc::raw::pack(value);
    if (serialized_value.size() > BTS_DNS_MAX_VALUE_LEN) {
        FC_ASSERT(!"Serialized value too long in update_record");
    }
    
    inputs = collect_inputs( asset(), total_in, req_sigs );
   
    for (auto pair : get_unspent_outputs())
    {
        if ( pair.second.claim_func == claim_domain
          && pair.second.as<claim_domain_output>().name == name)
        {
            trx.inputs.push_back( trx_input( get_ref_from_output_idx(pair.first) ) );
            break;
        }
        FC_ASSERT(!"Tried to update record but name UTXO not found in wallet");
    } 

    auto change_amt = total_in;

    auto domain_output = claim_domain_output();
    domain_output.name = name;
    domain_output.value = serialized_value;
    domain_output.owner = domain_addr;
    domain_output.flags = claim_domain_output::not_for_sale;

    trx.outputs.push_back( trx_output( domain_output, asset() ) );
    trx.outputs.push_back( trx_output( claim_by_signature_output( change_addr ), change_amt ) );

    trx.sigs.clear();
    sign_transaction(trx, req_sigs, false);

    trx = add_fee_and_sign(trx, asset(), total_in, req_sigs);

    return trx;

} FC_RETHROW_EXCEPTIONS(warn, "update_record ${name} with value ${val}", ("name", name)("val", value)) }


bts::blockchain::signed_transaction dns_wallet::sell_domain(
                                    const std::string& name, asset amount)
{ try {
    signed_transaction trx;
     // TODO check name length
   
    auto change_addr = new_recv_address("Change address");
    auto sale_addr = new_recv_address("Domain sale address");
    auto req_sigs = std::unordered_set<bts::blockchain::address>();
    auto inputs = std::vector<trx_input>();
    bts::blockchain::asset total_in; // set by collect_inputs
   
    inputs = collect_inputs( asset(), total_in, req_sigs );
   
    for (auto pair : get_unspent_outputs())
    {
        if ( pair.second.claim_func == claim_domain
          && pair.second.as<claim_domain_output>().name == name)
        {
            trx.inputs.push_back( trx_input( get_ref_from_output_idx(pair.first) ) );
            break;
        }
        FC_ASSERT(!"Tried to sell a name UTXO not found in wallet");
    } 

    auto change_amt = total_in;

    auto domain_output = claim_domain_output();
    domain_output.name = name;
    domain_output.value = std::vector<char>();
    domain_output.owner = sale_addr;
    domain_output.flags = claim_domain_output::for_auction;

    trx.outputs.push_back( trx_output( domain_output, amount ) );
    trx.outputs.push_back( trx_output( claim_by_signature_output( change_addr ), change_amt ) );

    trx.sigs.clear();
    sign_transaction(trx, req_sigs, false);

    trx = add_fee_and_sign(trx, asset(), total_in, req_sigs);

    return trx;


} FC_RETHROW_EXCEPTIONS(warn, "sell_domai ${name} with ${amt}", ("name", name)("amt", amount)) }


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
