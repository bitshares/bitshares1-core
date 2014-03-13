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
    enum flag_values
    {
        not_for_sale = 0,
        for_auction  = 1 // if output less than 3 days old, it is claimable
    };
    static const claim_type_enum type;

    claim_domain_output():flags(not_for_sale){}

    std::string                        name;
    std::vector<char>                  value; 
    bts::blockchain::address           owner;
    fc::enum_type<flag_values,uint8_t> flags;
};

}} //bts::dns

FC_REFLECT_ENUM(bts::dns::claim_type_enum, (claim_domain));
FC_REFLECT(bts::dns::claim_domain_input, (type));
FC_REFLECT(bts::dns::claim_domain_output, (type)(name)(value)(owner)(flags));
FC_REFLECT_ENUM(bts::dns::claim_domain_output::flag_values, (not_for_sale)(for_auction));
