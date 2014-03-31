#include <bts/dns/dns_util.hpp>

namespace bts { namespace dns {

bool is_dns_output(const trx_output &output)
{
    return output.claim_func == claim_domain;
}

claim_domain_output to_dns_output(const trx_output &output)
{
    FC_ASSERT(is_valid_amount(output.amount), "Invalid amount");

    auto dns_output = output.as<claim_domain_output>();

    FC_ASSERT(is_valid_name(dns_output.name), "Invalid name");
    FC_ASSERT(is_valid_value(dns_output.value), "Invalid value");
    FC_ASSERT(is_valid_state(dns_output.state), "Invalid state");

    return dns_output;
}

output_reference get_name_tx_ref(const std::string &name, dns_db &db)
{
    FC_ASSERT(is_valid_name(name), "Invalid name");

    return db.get_dns_record(name).last_update_ref;
}

trx_output get_tx_ref_output(const output_reference &tx_ref, dns_db &db)
{
    return db.fetch_output(tx_ref);
}

uint32_t get_tx_age(const output_reference &tx_ref, dns_db &db)
{
    auto tx_ref_id = tx_ref.trx_hash;
    auto block_num = db.fetch_trx_num(tx_ref_id).block_num;

    return db.head_block_num() - block_num;
}

uint32_t get_name_tx_age(const std::string &name, dns_db &db)
{
    FC_ASSERT(is_valid_name(name), "Invalid name");

    return get_tx_age(get_name_tx_ref(name, db), db);
}

bool is_auction_age(uint32_t age)
{
    return age < DNS_AUCTION_DURATION_BLOCKS;
}

bool is_expired_age(uint32_t age)
{
    return age >= DNS_EXPIRE_DURATION_BLOCKS;
}

bool is_useable_age(uint32_t age)
{
    return !is_auction_age(age) && !is_expired_age(age);
}

std::vector<std::string> get_unspent_names(const std::map<bts::wallet::output_index, trx_output> &unspent_outputs)
{
    std::vector<std::string> names = std::vector<std::string>();

    for (auto pair : unspent_outputs)
    {
        if (!is_dns_output(pair.second))
            continue;

        names.push_back(to_dns_output(pair.second).name);
    } 

    return names;
}

bool name_is_in_txs(const std::string &name, const signed_transactions &txs)
{
    FC_ASSERT(is_valid_name(name), "Invalid name");

    for (auto &tx : txs)
    {
        for (auto &output : tx.outputs)
        {
            if (!is_dns_output(output))
                continue;

            if (to_dns_output(output).name == name)
                return true;
        }
    }

    return false;
}

/* Check if name is available for bid: new, in auction, or expired */
bool name_is_available(const std::string &name, const signed_transactions &txs, dns_db &db, bool &new_or_expired,
                       output_reference &prev_tx_ref)
{
    FC_ASSERT(is_valid_name(name), "Invalid name");

    new_or_expired = false;

    if (name_is_in_txs(name, txs))
        return false;

    if (!db.has_dns_record(name))
    {
        new_or_expired = true;
        return true;
    }

    prev_tx_ref = get_name_tx_ref(name, db);
    auto prev_tx_age = get_tx_age(prev_tx_ref, db);

    if (is_expired_age(prev_tx_age))
        new_or_expired = true;

    return is_auction_age(prev_tx_age) || new_or_expired;
}

/* Check if name is available for value update or auction */
bool name_is_useable(const std::string &name, const signed_transactions &txs, dns_db &db,
                     const std::map<bts::wallet::output_index, trx_output> &unspent_outputs,
                     output_reference &prev_tx_ref)
{
    FC_ASSERT(is_valid_name(name), "Invalid name");

    if (name_is_in_txs(name, txs))
        return false;

    if (!db.has_dns_record(name))
        return false;

    prev_tx_ref = get_name_tx_ref(name, db);

    if (!is_useable_age(get_tx_age(prev_tx_ref, db)))
        return false;

    // Check if spendable
    std::vector<std::string> unspent_names = get_unspent_names(unspent_outputs);
    if (std::find(unspent_names.begin(), unspent_names.end(), name) == unspent_names.end())
        return false;

    return true;
}

std::vector<char> serialize_value(const fc::variant &value)
{
    return fc::raw::pack(value);
}

bool is_valid_amount(const asset &amount)
{
    return amount >= DNS_ASSET_ZERO;
}

bool is_valid_name(const std::string &name)
{
    return name.size() <= DNS_MAX_NAME_LEN;
}

bool is_valid_value(const std::vector<char> &value)
{
    return value.size() <= DNS_MAX_VALUE_LEN;
}

bool is_valid_value(const fc::variant &value)
{
    return is_valid_value(serialize_value(value));
}

bool is_valid_state(const fc::enum_type<uint8_t, claim_domain_output::states> state)
{
    return (state == claim_domain_output::possibly_in_auction) || (state == claim_domain_output::not_in_auction);
}

bool is_valid_bid_price(const asset &prev_bid_price, const asset &bid_price, asset &transfer_amount)
{
    FC_ASSERT(is_valid_amount(prev_bid_price));
    FC_ASSERT(is_valid_amount(bid_price));

    if (bid_price < DNS_MIN_BID_FROM(prev_bid_price.get_rounded_amount()))
        return false;

    transfer_amount = bid_price - DNS_BID_FEE_RATIO(bid_price - prev_bid_price);

    return true;
}

}} // bts::dns
