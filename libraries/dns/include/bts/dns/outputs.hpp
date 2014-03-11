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
    static const claim_type_enum type;
    std::string name;
    std::vector<char> value; 
    bts::blockchain::address owner;
};

}}
