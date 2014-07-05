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


    bool domain_in_auction( odomain_record rec )
    {
        auto now = fc::time_point::now().sec_since_epoch();
        if ( ! rec.valid() )
            return false;
        if ( now > rec->last_update + P2P_AUCTION_DURATION_SECS )
            return false;
        if ( rec->update_type == domain_record::first_bid )
            return true;
        if ( rec->update_type == domain_record::bid )
            return true;
        if ( rec->update_type == domain_record::sell )
            return true;
        return false;
    }

    /* Is ownership actually locked in? (auction over, not expired) */
    bool domain_owned_by_owner( odomain_record rec )
    {
        auto now = fc::time_point::now().sec_since_epoch();
        if ( ! rec.valid() )
        {
            ilog("AAA invalid record");
            return false;
        }
        if ( domain_in_auction(rec) )
        {
            ilog("AAA in auction");
            return false;
        }
        if ( now > rec->last_update + P2P_EXPIRE_DURATION_SECS )
        {
            ilog("AAA expired");
            return false;
        }
        return true;
    }
}} // bts::blockchain
