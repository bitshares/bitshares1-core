#include <bts/dns/dns_wallet.hpp>
#include <bts/dns/outputs.hpp>
#include <bts/blockchain/config.hpp>

#include<fc/reflect/variant.hpp>

namespace bts { namespace dns {
using namespace bts::blockchain;
bts::blockchain::signed_transaction dns_wallet::buy_domain(
                    const std::string& name, bts::blockchain::asset amount, dns_db& db)
{ try {
    signed_transaction trx;
    // TODO check name length
   
    // new_addr = ...

    // if there is record and it's not expired and it's for sale, make a bid
    if (db.has_dns_record(name))
    {
        // check "for sale" state
        auto old_record = db.get_dns_record(name);
        auto old_utxo_ref = old_record.last_update_ref;
        auto old_output = db.fetch_output(old_utxo_ref);
        auto dns_output = old_output.as<claim_domain_output>();
        if (dns_output.flags != claim_domain_output::for_auction)
        {
            FC_ASSERT(!"tried to make a bid for a name that is not for sale");
        }
        auto block_num = db.fetch_trx_num(old_utxo_ref.trx_hash).block_num;
        auto current_block = db.head_block_num();
        if (current_block - block_num < BTS_BLOCKCHAIN_BLOCKS_PER_YEAR)
        {
            // make a bid
            return trx;
        }
    }
    // otherwise, you're starting a new auction
    // * set flags=for_sale
    // * set amount=amount  
    // * set owner=new_addr
  
} FC_RETHROW_EXCEPTIONS(warn, "buy_domain ${name} with ${amt}", ("name", name)("amt", amount)) }


bts::blockchain::signed_transaction dns_wallet::update_record(
                                             const std::string& name, address addr,   
                                             fc::variant value)
{ try {
    // * check value length
    // * check name exists
    // * check that auction is over
    // * check that you own it
    // set owner=addr
    // set value=value
} FC_RETHROW_EXCEPTIONS(warn, "update_record ${name} with value ${val}", ("name", name)("val", value)) }


bts::blockchain::signed_transaction dns_wallet::sell_domain(
                                    const std::string& name, asset amount)
{ try {
    // * check taht you own it
    // set for_sale
    // set owner to you
} FC_RETHROW_EXCEPTIONS(warn, "sell_domai ${name} with ${amt}", ("name", name)("amt", amount)) }


void dns_wallet::scan_output( const trx_output& out,
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
                }
            }
            default:
            {
                wallet::scan_output( out, ref, oidx );
                break;
            }
        }
    } FC_RETHROW_EXCEPTIONS( warn, "" )
}

}} // bts::dns
