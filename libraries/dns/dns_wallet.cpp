#include <bts/dns/dns_wallet.hpp>

namespace bts { namespace dns {

dns_wallet::dns_wallet(const dns_db_ptr& db) : _db(db)
{
    FC_ASSERT(db != nullptr);
    _transaction_validator = std::dynamic_pointer_cast<dns_transaction_validator>(db->get_transaction_validator());
    FC_ASSERT(_transaction_validator != nullptr);
}

dns_wallet::~dns_wallet()
{
}

signed_transaction dns_wallet::bid(const std::string& key, const asset& bid_price,
                                   const signed_transactions& pending_txs)
{ try {
    FC_ASSERT(_transaction_validator->is_valid_key(key), "Invalid key");

    /* Key should be new, for auction, or expired */
    bool new_or_expired;
    output_reference prev_tx_ref;
    FC_ASSERT(key_is_available(key, pending_txs, new_or_expired, prev_tx_ref), "Key not available");

    signed_transaction tx;
    claim_dns_output dns_output(key, claim_dns_output::last_tx_type_enum::auction, new_receive_address("Key: " + key));

    if (new_or_expired)
    {
        tx.outputs.push_back(trx_output(dns_output, bid_price));
    }
    else
    {
        tx.inputs.push_back(trx_input(prev_tx_ref));

        auto prev_output = _db->fetch_output(prev_tx_ref);
        FC_ASSERT(_transaction_validator->is_valid_bid_price(bid_price, prev_output.amount), "Invalid bid price");
        auto transfer_amount = _transaction_validator->get_bid_transfer_amount(bid_price, prev_output.amount);

        /* Fee is implicit from difference */
        auto prev_dns_output = to_dns_output(prev_output);
        tx.outputs.push_back(trx_output(claim_by_signature_output(prev_dns_output.owner), transfer_amount));
        tx.outputs.push_back(trx_output(dns_output, bid_price));
    }

    return collect_inputs_and_sign(tx, bid_price);
} FC_RETHROW_EXCEPTIONS(warn, "bid on key ${k} with price ${p}", ("k", key) ("p", bid_price)); }

signed_transaction dns_wallet::ask(const std::string& key, const asset& ask_price,
                                   const signed_transactions& pending_txs)
{ try {
    FC_ASSERT(_transaction_validator->is_valid_key(key), "Invalid key");

    /* Key should exist and be owned */
    output_reference prev_tx_ref;
    FC_ASSERT(key_is_useable(key, pending_txs, prev_tx_ref), "Key unavailable");

    auto prev_output = _db->fetch_output(prev_tx_ref);
    auto prev_dns_output = to_dns_output(prev_output);

    claim_dns_output dns_output(key, claim_dns_output::last_tx_type_enum::auction, prev_dns_output.owner);

    return update(dns_output, ask_price, prev_tx_ref, prev_dns_output.owner);
} FC_RETHROW_EXCEPTIONS(warn, "ask on key ${k} with price ${p}", ("k", key) ("p", ask_price)); }

signed_transaction dns_wallet::transfer(const std::string& key, const address& recipient,
                                        const signed_transactions& pending_txs)
{ try {
    FC_ASSERT(_transaction_validator->is_valid_key(key), "Invalid key");

    /* Key should exist and be owned */
    output_reference prev_tx_ref;
    FC_ASSERT(key_is_useable(key, pending_txs, prev_tx_ref), "Key unavailable");

    auto prev_output = _db->fetch_output(prev_tx_ref);
    auto prev_dns_output = to_dns_output(prev_output);

    claim_dns_output dns_output(key, claim_dns_output::last_tx_type_enum::update, recipient, prev_dns_output.value);

    return update(dns_output, prev_output.amount, prev_tx_ref, prev_dns_output.owner);
} FC_RETHROW_EXCEPTIONS(warn, "transfer key ${k} with recipient ${r}", ("k", key) ("r", recipient)); }

signed_transaction dns_wallet::release(const std::string& key,
                                       const signed_transactions& pending_txs)
{ try {
    FC_ASSERT(_transaction_validator->is_valid_key(key), "Invalid key");

    /* Key should exist and be owned */
    output_reference prev_tx_ref;
    FC_ASSERT(key_is_useable(key, pending_txs, prev_tx_ref), "Key unavailable");

    auto prev_output = _db->fetch_output(prev_tx_ref);
    auto prev_dns_output = to_dns_output(prev_output);

    claim_dns_output dns_output(key, claim_dns_output::last_tx_type_enum::release, prev_dns_output.owner);

    return update(dns_output, prev_output.amount, prev_tx_ref, prev_dns_output.owner);
} FC_RETHROW_EXCEPTIONS(warn, "release key ${k}", ("k", key)); }

signed_transaction dns_wallet::set(const std::string& key, const fc::variant& value,
                                   const signed_transactions& pending_txs)
{ try {
    FC_ASSERT(_transaction_validator->is_valid_key(key), "Invalid key");
    FC_ASSERT(_transaction_validator->is_valid_value(serialize_value(value)), "Invalid value");

    /* Key should exist and be owned */
    output_reference prev_tx_ref;
    FC_ASSERT(key_is_useable(key, pending_txs, prev_tx_ref), "Key unavailable");

    auto prev_output = _db->fetch_output(prev_tx_ref);
    auto prev_dns_output = to_dns_output(prev_output);

    claim_dns_output dns_output(key, claim_dns_output::last_tx_type_enum::update, prev_dns_output.owner, serialize_value(value));

    return update(dns_output, prev_output.amount, prev_tx_ref, prev_dns_output.owner);
} FC_RETHROW_EXCEPTIONS(warn, "set key ${k} with value ${v}", ("k", key) ("v", value)); }

// TODO: Also check current pending_txs
fc::variant dns_wallet::lookup(const std::string& key,
                               const signed_transactions& pending_txs)
{ try {
    FC_ASSERT(_transaction_validator->is_valid_key(key), "Invalid key");
    FC_ASSERT(_db->has_dns_ref(key), "Key does not exist");

    auto output = _db->fetch_output(_db->get_dns_ref(key));

    return unserialize_value(to_dns_output(output).value);
} FC_RETHROW_EXCEPTIONS(warn, "lookup key ${k}", ("k", key)); }

// TODO: Also check current pending_txs
std::vector<trx_output> dns_wallet::get_active_auctions()
{
    std::vector<trx_output> list;

    auto f = [](const std::string& k, const output_reference& r, dns_db& db)->bool
    {
        auto transaction_validator = std::dynamic_pointer_cast<dns_transaction_validator>(db.get_transaction_validator());
        FC_ASSERT(transaction_validator != nullptr);
        return !transaction_validator->auction_is_closed(r);
    };

    auto map = _db->filter(f);

    for (auto iter = map.begin(); iter != map.end(); iter++)
        list.push_back(_db->fetch_output(iter->second));

    return list;
}

std::string dns_wallet::get_input_info_string(bts::blockchain::chain_database& db, const trx_input& in)
{
    //TODO this should print info about what we're claiming the output as (expired/auction etc)
    return get_output_info_string(db.fetch_output(in.output_ref));
}

std::string dns_wallet::get_output_info_string(const trx_output& out)
{
    if (!is_dns_output(out))
        return wallet::get_output_info_string(out);

    std::stringstream ret;
    auto dns_out = to_dns_output(out);
    std::string owner_str = dns_out.owner;
    ret << "DNS Claim\n  Key: \"" << dns_out.key << "\"\n  Owner: " <<
       owner_str << "\n  Amount: " << std::string(out.amount);
    return ret.str();
}

bool dns_wallet::scan_output(transaction_state& state, const trx_output& out, const output_reference& ref,
                             const output_index& idx)
{
    if (!is_dns_output(out))
        return wallet::scan_output(state, out, ref, idx);

    auto dns_output = to_dns_output(out);

    if (is_my_address(dns_output.owner))
    {
        cache_output(state.trx.vote, out, ref, idx);
        return true;
    }

    return false;
}

signed_transaction dns_wallet::update(const claim_dns_output& dns_output, const asset& amount,
                                      const output_reference& prev_tx_ref, const address& prev_owner)
{ try {
    signed_transaction tx;
    std::unordered_set<address> req_sigs;

    tx.inputs.push_back(trx_input(prev_tx_ref));
    tx.outputs.push_back(trx_output(dns_output, amount));
    req_sigs.insert(prev_owner);

    return collect_inputs_and_sign(tx, asset(), req_sigs);
} FC_RETHROW_EXCEPTIONS(warn, "update on key ${k} with amount ${a}", ("k", dns_output.key) ("a", amount)); }

std::vector<std::string> dns_wallet::get_keys_from_txs(const signed_transactions& txs)
{
    std::vector<std::string> keys;

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

std::vector<std::string> dns_wallet::get_keys_from_unspent(const std::map<output_index, trx_output>& unspent_outputs)
{
    std::vector<std::string> keys;

    for (auto pair : unspent_outputs)
    {
        if (!is_dns_output(pair.second))
            continue;

        keys.push_back(to_dns_output(pair.second).key);
    }

    return keys;
}

bool dns_wallet::key_is_available(const std::string& key, const signed_transactions& pending_txs, bool& new_or_expired,
                                  output_reference& prev_tx_ref)
{
    return _transaction_validator->key_is_available(key, get_keys_from_txs(pending_txs), new_or_expired, prev_tx_ref);
}

bool dns_wallet::key_is_useable(const std::string& key, const signed_transactions& pending_txs, output_reference& prev_tx_ref)
{
    return _transaction_validator->key_is_useable(key, get_keys_from_txs(pending_txs),
            get_keys_from_unspent(get_unspent_outputs()), prev_tx_ref);
}

} } // bts::dns
