#include <bts/blockchain/types.hpp>
#include <bts/blockchain/dns_config.hpp>
#include <bts/blockchain/dns_utils.hpp>
#include <fc/time.hpp>

namespace bts { namespace blockchain {

    bool is_valid_domain( const std::string& str )
    {
        if( str.size() < P2P_MIN_DOMAIN_NAME_SIZE ) return false;
        if( str.size() > P2P_MAX_DOMAIN_NAME_SIZE ) return false;
        if( str[0] < 'a' || str[0] > 'z' ) return false;
        for( const auto& c : str )
        {
            if( c >= 'a' && c <= 'z' ) continue;
            else if( c >= '0' && c <= '9' ) continue;
            else if( c == '-' ) continue;
            // else if( c == '.' ) continue;   TODO subdomain logic
            else return false;
        }
        return true;
    }

}} // bts::blockchain
