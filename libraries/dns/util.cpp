#include <bts/dns/util.hpp>

namespace bts { namespace dns {

bool is_dns_output(const trx_output &output)
{
    return output.claim_func == claim_dns;
}

claim_dns_output to_dns_output(const trx_output &output)
{
    auto dns_output = output.as<claim_dns_output>();
    FC_ASSERT(dns_output.is_valid(), "Invalid DNS output");

    return dns_output;
}

output_reference get_key_tx_ref(const std::string &key, dns_db &db)
{
    return db.get_dns_ref(key);
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
    FC_ASSERT(is_dns_output(output));

    auto dns_output = to_dns_output(output);
    auto age = get_tx_age(tx_ref, db);

    if (dns_output.last_tx_type == claim_dns_output::last_tx_type_enum::auction
        && age < DNS_AUCTION_DURATION_BLOCKS)
        return false;

    return true;
}

bool key_is_expired(const output_reference &tx_ref, dns_db &db)
{
    auto output = get_tx_ref_output(tx_ref, db);
    FC_ASSERT(is_dns_output(output));

    if (!auction_is_closed(tx_ref, db))
        return false;

    auto dns_output = to_dns_output(output);
    auto age = get_tx_age(tx_ref, db);

    if (dns_output.last_tx_type == claim_dns_output::last_tx_type_enum::auction)
        return age >= (DNS_AUCTION_DURATION_BLOCKS + DNS_EXPIRE_DURATION_BLOCKS);

    FC_ASSERT(dns_output.last_tx_type == claim_dns_output::last_tx_type_enum::update);

    return age >= DNS_EXPIRE_DURATION_BLOCKS;
}

std::vector<std::string> get_keys_from_txs(const signed_transactions &txs)
{
    std::vector<std::string> keys = std::vector<std::string>();

    for (auto &tx : txs)
    {
        for (auto &output : tx.outputs)
        {
            if (!is_dns_output(output))
                continue;

            keys.push_back(to_dns_output(output).key);
        }
    }

    return keys;
}

std::vector<std::string> get_keys_from_unspent(const std::map<bts::wallet::output_index, trx_output>
                                                &unspent_outputs)
{
    std::vector<std::string> keys = std::vector<std::string>();

    for (auto pair : unspent_outputs)
    {
        if (!is_dns_output(pair.second))
            continue;

        keys.push_back(to_dns_output(pair.second).key);
    }

    return keys;
}

/* Check if key is available for bid: new, in auction, or expired */
bool key_is_available(const std::string &key, const std::vector<std::string> &key_pool, dns_db &db,
                       bool &new_or_expired, output_reference &prev_tx_ref)
{
    new_or_expired = false;

    if (std::find(key_pool.begin(), key_pool.end(), key) != key_pool.end())
        return false;

    if (!db.has_dns_ref(key))
    {
        new_or_expired = true;
        return true;
    }

    prev_tx_ref = get_key_tx_ref(key, db);

    if (key_is_expired(prev_tx_ref, db))
        new_or_expired = true;

    return !auction_is_closed(prev_tx_ref, db) || new_or_expired;
}

bool key_is_available(const std::string &key, const signed_transactions &pending_txs, dns_db &db,
                       bool &new_or_expired, output_reference &prev_tx_ref)
{
    return key_is_available(key, get_keys_from_txs(pending_txs), db, new_or_expired, prev_tx_ref);
}

/* Check if key is available for value update or auction */
bool key_is_useable(const std::string &key, const signed_transactions &pending_txs, dns_db &db,
                     const std::map<bts::wallet::output_index, trx_output> &unspent_outputs,
                     output_reference &prev_tx_ref)
{
    std::vector<std::string> key_pool = get_keys_from_txs(pending_txs);
    if (std::find(key_pool.begin(), key_pool.end(), key) != key_pool.end())
        return false;

    if (!db.has_dns_ref(key))
        return false;

    prev_tx_ref = get_key_tx_ref(key, db);

    if (!auction_is_closed(prev_tx_ref, db))
        return false;

    if (key_is_expired(prev_tx_ref, db))
        return false;

    /* Check if spendable */
    std::vector<std::string> unspent_keys = get_keys_from_unspent(unspent_outputs);
    if (std::find(unspent_keys.begin(), unspent_keys.end(), key) == unspent_keys.end())
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

bool is_valid_bid_price(const asset &bid_price, const asset &prev_bid_price)
{
    if (bid_price < asset(DNS_MIN_BID_FROM(prev_bid_price.amount), prev_bid_price.unit))
        return false;

    return true;
}

asset get_bid_transfer_amount(const asset &bid_price, const asset &prev_bid_price)
{
    FC_ASSERT(is_valid_bid_price(bid_price, prev_bid_price), "Invalid bid price");

    return bid_price - DNS_BID_FEE_RATIO(bid_price - prev_bid_price);
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
