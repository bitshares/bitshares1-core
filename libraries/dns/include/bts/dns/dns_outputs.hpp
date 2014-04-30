#pragma once

#include <bts/blockchain/transaction.hpp>

namespace bts { namespace dns {

using namespace bts::blockchain;

enum claim_type_enum
{
    // 20->29 reserved for BitShares DNS
    claim_dns = 20, /* Basic claim by single address */
};

struct claim_dns_input
{
    static const claim_type_enum type;
};

struct claim_dns_output
{
    static const claim_type_enum type;

    enum class last_tx_type_enum : uint8_t
    {
        auction,
        update,
        release
    };

    std::string         key;
    last_tx_type_enum   last_tx_type;
    address             owner;
    std::vector<char>   value;

    claim_dns_output();
    claim_dns_output(const std::string& k, const last_tx_type_enum& l, const address& o,
                     const std::vector<char>& v = std::vector<char>());
};

bool is_dns_output(const trx_output& output);
claim_dns_output to_dns_output(const trx_output& output);

std::vector<char> serialize_value(const fc::variant& value);
fc::variant unserialize_value(const std::vector<char>& value);

} } //bts::dns

FC_REFLECT_ENUM(bts::dns::claim_type_enum, (claim_dns));
FC_REFLECT(bts::dns::claim_dns_input, BOOST_PP_SEQ_NIL);
FC_REFLECT(bts::dns::claim_dns_output, (key)(last_tx_type)(owner)(value));
FC_REFLECT_ENUM(bts::dns::claim_dns_output::last_tx_type_enum, (auction)(update)(release));
