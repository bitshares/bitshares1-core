#include <bts/dns/outputs.hpp>
#include <bts/dns/util.hpp>

namespace bts { namespace dns {

const claim_type_enum claim_dns_input::type = claim_type_enum::claim_dns;
const claim_type_enum claim_dns_output::type = claim_type_enum::claim_dns;

claim_dns_output::claim_dns_output(const std::string& k, const last_tx_type_enum& l,
                                         const address& o, const std::vector<char>& v)
                                         : key(k), last_tx_type(l), owner(o), value(v)
{
    FC_ASSERT(is_valid(), "claim_dns_output improperly initialized");
}

bool claim_dns_output::is_valid() const
{
    if (!has_valid_key()) return false;
    if (!has_valid_value()) return false;

    return true;
}

bool claim_dns_output::has_valid_key() const
{
    return key.size() <= DNS_MAX_KEY_LEN;
}

bool claim_dns_output::has_valid_value() const
{
    return value.size() <= DNS_MAX_VALUE_LEN;
}

} } // bts::dns
