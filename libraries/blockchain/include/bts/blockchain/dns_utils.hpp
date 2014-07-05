#pragma once
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/dns_record.hpp>

namespace bts { namespace blockchain {

    bool is_valid_domain( const std::string& domain_name );
    bool domain_in_auction( odomain_record rec );
    bool domain_owned_by_owner( odomain_record rec );

}}
