#include <bts/dns/util.hpp>

namespace bts { namespace dns {

bool is_domain_output(const trx_output &output)
{
    return output.claim_func == claim_domain;
}

claim_domain_output to_domain_output(const trx_output &output)
{
    auto dns_output = output.as<claim_domain_output>();

    FC_ASSERT(is_valid_name(dns_output.name), "Invalid name");
    FC_ASSERT(is_valid_value(dns_output.value), "Invalid value");
    FC_ASSERT(is_valid_owner(dns_output.owner), "Invalid owner");
    FC_ASSERT(is_valid_last_tx_type(dns_output.last_tx_type), "Invalid last_tx_type");

    return dns_output;
}

output_reference get_name_tx_ref(const std::string &name, dns_db &db)
{
    FC_ASSERT(is_valid_name(name), "Invalid name");

    return db.get_dns_ref(name);
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

bool auction_is_closed(const output_reference &tx_ref, dns_db &db)
{
    auto output = get_tx_ref_output(tx_ref, db);
    FC_ASSERT(is_domain_output(output));

    auto dns_output = to_domain_output(output);
    auto age = get_tx_age(tx_ref, db);

    if (dns_output.last_tx_type == claim_domain_output::bid_or_auction
        && age < DNS_AUCTION_DURATION_BLOCKS)
        return false;

    return true;
}

bool domain_is_expired(const output_reference &tx_ref, dns_db &db)
{
    auto output = get_tx_ref_output(tx_ref, db);
    FC_ASSERT(is_domain_output(output));

    if (!auction_is_closed(tx_ref, db))
        return false;

    auto dns_output = to_domain_output(output);
    auto age = get_tx_age(tx_ref, db);

    if (dns_output.last_tx_type == claim_domain_output::bid_or_auction)
        return age >= (DNS_AUCTION_DURATION_BLOCKS + DNS_EXPIRE_DURATION_BLOCKS);

    FC_ASSERT(dns_output.last_tx_type == claim_domain_output::update);

    return age >= DNS_EXPIRE_DURATION_BLOCKS;
}

std::vector<std::string> get_names_from_txs(const signed_transactions &txs)
{
    std::vector<std::string> names = std::vector<std::string>();

    for (auto &tx : txs)
    {
        for (auto &output : tx.outputs)
        {
            if (!is_domain_output(output))
                continue;

            names.push_back(to_domain_output(output).name);
        }
    }

    return names;
}

std::vector<std::string> get_names_from_unspent(const std::map<bts::wallet::output_index, trx_output>
                                                &unspent_outputs)
{
    std::vector<std::string> names = std::vector<std::string>();

    for (auto pair : unspent_outputs)
    {
        if (!is_domain_output(pair.second))
            continue;

        names.push_back(to_domain_output(pair.second).name);
    }

    return names;
}

/* Check if name is available for bid: new, in auction, or expired */
bool name_is_available(const std::string &name, const std::vector<std::string> &name_pool, dns_db &db,
                       bool &new_or_expired, output_reference &prev_tx_ref)
{
    FC_ASSERT(is_valid_name(name), "Invalid name");

    new_or_expired = false;

    if (std::find(name_pool.begin(), name_pool.end(), name) != name_pool.end())
        return false;

    if (!db.has_dns_ref(name))
    {
        new_or_expired = true;
        return true;
    }

    prev_tx_ref = get_name_tx_ref(name, db);

    if (domain_is_expired(prev_tx_ref, db))
        new_or_expired = true;

    return !auction_is_closed(prev_tx_ref, db) || new_or_expired;
}

bool name_is_available(const std::string &name, const signed_transactions &tx_pool, dns_db &db,
                       bool &new_or_expired, output_reference &prev_tx_ref)
{
    FC_ASSERT(is_valid_name(name), "Invalid name");

    return name_is_available(name, get_names_from_txs(tx_pool), db, new_or_expired, prev_tx_ref);
}

/* Check if name is available for value update or auction */
bool name_is_useable(const std::string &name, const signed_transactions &tx_pool, dns_db &db,
                     const std::map<bts::wallet::output_index, trx_output> &unspent_outputs,
                     output_reference &prev_tx_ref)
{
    FC_ASSERT(is_valid_name(name), "Invalid name");

    std::vector<std::string> name_pool = get_names_from_txs(tx_pool);
    if (std::find(name_pool.begin(), name_pool.end(), name) != name_pool.end())
        return false;

    if (!db.has_dns_ref(name))
        return false;

    prev_tx_ref = get_name_tx_ref(name, db);

    if (!auction_is_closed(prev_tx_ref, db))
        return false;

    if (domain_is_expired(prev_tx_ref, db))
        return false;

    /* Check if spendable */
    std::vector<std::string> unspent_names = get_names_from_unspent(unspent_outputs);
    if (std::find(unspent_names.begin(), unspent_names.end(), name) == unspent_names.end())
        return false;

    return true;
}

std::vector<char> serialize_value(const fc::variant &value)
{
    return fc::raw::pack(value);
}

fc::variant unserialize_value(const std::vector<char> &value)
{
    return fc::raw::unpack<fc::variant>(value);
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

bool is_valid_owner(const bts::blockchain::address &owner)
{
    return owner.is_valid();
}

bool is_valid_last_tx_type(const fc::enum_type<uint8_t, claim_domain_output::last_tx_type_enum> &last_tx_type)
{
    return (last_tx_type == claim_domain_output::bid_or_auction) || (last_tx_type == claim_domain_output::update);
}

bool is_valid_bid_price(const asset &bid_price, const asset &prev_bid_price)
{
    if (bid_price < DNS_MIN_BID_FROM(prev_bid_price.get_rounded_amount()))
        return false;

    return true;
}

bool is_valid_ask_price(const asset &ask_price)
{
    return true;
}

asset get_bid_transfer_amount(const asset &bid_price, const asset &prev_bid_price)
{
    FC_ASSERT(is_valid_bid_price(bid_price, prev_bid_price), "Invalid bid price");

    return bid_price - DNS_BID_FEE_RATIO(bid_price - prev_bid_price);
}

fc::variant lookup_value(const std::string& key, dns_db& db)
{
    FC_ASSERT(is_valid_name(key));
    FC_ASSERT(db.has_dns_ref(key), "Key does not exist");

    auto output = get_tx_ref_output(db.get_dns_ref(key), db);

    return unserialize_value(to_domain_output(output).value);
}

std::vector<trx_output> get_active_auctions(dns_db& db)
{
    std::vector<trx_output> list;

    auto f = [](const std::string& k, const output_reference& v, dns_db& db)->bool { return !auction_is_closed(v, db); };
    auto map = db.filter(f);

    for (auto iter = map.begin(); iter != map.end(); iter++)
        list.push_back(get_tx_ref_output(iter->second, db));

    return list;
}

} } // bts::dns
