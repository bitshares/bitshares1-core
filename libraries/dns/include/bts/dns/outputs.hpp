#pragma once
#include <bts/blockchain/outputs.hpp>

namespace bts { namespace dns {

enum claim_type_enum
{
   /** basic claim by single address */
   claim_domain         = 20,
};

struct claim_domain_input
{
    static const claim_type_enum type;
};

struct claim_domain_output
{
    enum states
    {
        not_in_auction,
        possibly_in_auction
    };

    static const claim_type_enum type;
    
    claim_domain_output():state(not_in_auction){} // TODO: do we need this

    std::string                    name;
    std::vector<char>              value; 
    bts::blockchain::address       owner;
    fc::enum_type<uint8_t, states> state;
};

}} //bts::dns

FC_REFLECT_ENUM(bts::dns::claim_type_enum, (claim_domain));
FC_REFLECT(bts::dns::claim_domain_input, BOOST_PP_SEQ_NIL);
FC_REFLECT(bts::dns::claim_domain_output, (name)(value)(owner)(state));
FC_REFLECT_ENUM(bts::dns::claim_domain_output::states, (not_in_auction)(possibly_in_auction));
