#include <bts/dns/dns_util.hpp>

namespace bts { namespace dns {

bool is_claim_domain(const trx_output &output)
{
    return output.claim_func == claim_domain;
}

claim_domain_output output_to_dns(const trx_output &output)
{
    return output.as<claim_domain_output>();
}

bool name_in_txs(const std::string &name, const signed_transactions &txs)
{
    FC_ASSERT(is_valid_name(name), "Invalid name");

    for (auto &tx : txs)
    {
        for (auto &output : tx.outputs)
        {
            if (!is_claim_domain(output))
                continue;

            if (output_to_dns(output).name == name)
                return true;
        }
    }

    return false;
}

/* Check if name is available for bid: new, in auction, or expired */
bool can_bid_on_name(const std::string &name, const signed_transactions &txs, dns_db &db, bool &name_exists,
                     trx_output &prev_output)
{
    FC_ASSERT(is_valid_name(name), "Invalid name");

    if (name_in_txs(name, txs))
    {
        name_exists = true;
        return false;
    }

    name_exists = db.has_dns_record(name);
    if (!name_exists)
        return true;

    // TODO: Separate function for age calculation
    auto record = db.get_dns_record(name);
    auto tx_ref = record.last_update_ref;
    prev_output = db.fetch_output(tx_ref);

    auto tx_ref_id = tx_ref.trx_hash;
    auto block_num = db.fetch_trx_num(tx_ref_id).block_num;
    auto age = db.head_block_num() - block_num;

    return !((age >= DNS_AUCTION_DURATION_BLOCKS) && (age < DNS_EXPIRE_DURATION_BLOCKS));
}

std::vector<char> serialize_value(const fc::variant &value)
{
    return fc::raw::pack(value);
}

bool is_valid_name(const std::string &name)
{
    return name.size() <= DNS_MAX_NAME_LEN;
}

bool is_valid_amount(const asset &amount)
{
    return amount >= DNS_ASSET_ZERO;
}

bool is_valid_value(const std::vector<char> &value)
{
    return value.size() <= DNS_MAX_VALUE_LEN;
}

bool is_valid_value(const fc::variant &value)
{
    return is_valid_value(serialize_value(value));
}

bool is_valid_bid(const trx_output &output, const signed_transactions &txs, dns_db &db, bool &name_exists,
                  trx_output &prev_output)
{
    if (!is_claim_domain(output))
        return false;

    auto dns_output = output_to_dns(output);
    if (dns_output.state == claim_domain_output::not_in_auction)
        return false;

    FC_ASSERT(dns_output.state == claim_domain_output::possibly_in_auction, "Invalid output state");

    return can_bid_on_name(dns_output.name, txs, db, name_exists, prev_output);
}

}} // bts::dns
