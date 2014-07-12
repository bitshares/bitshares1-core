#pragma once
#include <bts/blockchain/types.hpp>
#include <bts/blockchain/dns_record.hpp>

namespace bts { namespace blockchain {

    bool is_valid_domain( const std::string& domain_name );
    bool is_valid_value( const variant& value );

}}
