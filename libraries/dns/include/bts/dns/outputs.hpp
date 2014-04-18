#pragma once

#include <bts/blockchain/outputs.hpp>

namespace bts { namespace dns {

enum claim_type_enum
{
    // 20->29 reserved for BitShares DNS
    claim_domain = 20, /* Basic claim by single address */
};

struct claim_domain_input
{
    static const claim_type_enum type;
};

struct claim_domain_output
{
    enum last_tx_type_enum
    {
        bid_or_auction,
        update
    };

    static const claim_type_enum type;

    claim_domain_output():last_tx_type(bid_or_auction){}

    std::string                                 name;
    std::vector<char>                           value;
    bts::blockchain::address                    owner;
    fc::enum_type<uint8_t, last_tx_type_enum>   last_tx_type;
};

} } //bts::dns

FC_REFLECT_ENUM(bts::dns::claim_type_enum, (claim_domain));
FC_REFLECT(bts::dns::claim_domain_input, BOOST_PP_SEQ_NIL);
FC_REFLECT(bts::dns::claim_domain_output, (name)(value)(owner)(last_tx_type));
FC_REFLECT_ENUM(bts::dns::claim_domain_output::last_tx_type_enum, (bid_or_auction)(update));
